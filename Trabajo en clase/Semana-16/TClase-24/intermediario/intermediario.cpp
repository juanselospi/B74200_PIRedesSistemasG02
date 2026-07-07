#include "intermediario.h"
#include "Logger.h"

#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>

// constructor
IntermediaryServer::IntermediaryServer(const std::string & bindIp, int clientPort, const std::string & figureServerIp, int figureServerPort, const std::string & broadcastAddr, const std::string & seedPeerIp, int seedPeerPort ) : bindIp(bindIp), clientPort(clientPort), figureServerIp(figureServerIp), figureServerPort(figureServerPort), broadcastAddr(broadcastAddr), seedPeerIp(seedPeerIp), seedPeerPort(seedPeerPort) {

}

// punto de entrada principal
void IntermediaryServer::run() {
    Logger::log("INTERMEDIARY", "START", "bindIp=" + bindIp + " clientPort=" + std::to_string(clientPort) + " figureServer=" + figureServerIp + ":" + std::to_string(figureServerPort));

    // si me pasaron un intermediario por linea de comandos lo agrego de una
    if(!seedPeerIp.empty()){

        std::lock_guard<std::mutex> lock(httpPeersMutex);
        httpPeers.push_back(HttpPeer{ seedPeerIp, seedPeerPort, false });
        Logger::log("INTERMEDIARY", "SEED_PEER", "Intermediario remoto sembrado: " + seedPeerIp + ":" + std::to_string(seedPeerPort));
    }

    // armo la tabla en un hilo aparte para no bloquear la atencion de clientes
    std::thread initThread([this]() {
        std::vector<HttpPeer> known;
        { std::lock_guard<std::mutex> lock(httpPeersMutex); known = httpPeers; }
        for(const auto & hp : known) addHttpPeerAndMerge(hp.ip, hp.port);

        if(figureServerIp == "auto" || figureServerIp.empty()) {
            discoverMyServer();
        }
            
        buildLocalRouteTable();
        printRouteTable();
    });

    initThread.detach();

    // descubrimiento propio
    std::thread udpThread([this]() { listenJoinUdp(); }); udpThread.detach();
    std::thread bcastThread([this]() { broadcastJoinUdp(); }); bcastThread.detach();
    std::thread peerThread([this]() { listenPeerTcp(); }); peerThread.detach();

    // descubrimiento con los otros intermediarios INTER_HERE
    std::thread announceThread([this]() { announceToPeers(); }); announceThread.detach();
    std::thread peerListenThread([this]() { listenHttpPeers(); }); peerListenThread.detach();

    listenClients();
}

// pide las figuras a mi servidor y las mete en la tabla. reintenta si todavia no responde
void IntermediaryServer::buildLocalRouteTable() {

    Logger::log("INTERMEDIARY", "ROUTE_TABLE", "Construyendo tabla de rutas local desde " + figureServerIp + ":" + std::to_string(figureServerPort));

    const int maxAttempts = 60;

    for(int attempt = 1; attempt <= maxAttempts; ++attempt) {

        std::vector<std::string> figures = fetchFigureList(figureServerIp, figureServerPort);

        if(!figures.empty()) {

            std::lock_guard<std::mutex> lock(routeTableMutex);

            for(const auto & fig : figures) {

                FigureRoute route;
                route.figureName = fig;
                route.serverIp = figureServerIp;
                route.serverPort = figureServerPort;
                route.kind = RouteKind::LOCAL_PT;
                routeTable[fig] = route;
            }

            Logger::log("INTERMEDIARY", "ROUTE_TABLE", "figuras locales registradas: " + std::to_string(figures.size()) + " (total en tabla: " + std::to_string(routeTable.size()) + ")");
            return;
        }

        Logger::log("INTERMEDIARY", "ROUTE_TABLE", "servidor aun no responde, intento " + std::to_string(attempt) + "/" + std::to_string(maxAttempts));
        sleep(2);
    }

    Logger::log("INTERMEDIARY", "ERROR", "no se pudieron obtener figuras de mi servidor tras varios intentos");
}

// busca mi servidor por UDP -> mando "LEGO_DISCOVER" hasta que conteste "LEGO_SERVER <puerto>"
bool IntermediaryServer::discoverMyServer()
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0){

        Logger::log("INTERMEDIARY", "ERROR", "discoverMyServer: no se pudo crear socket UDP");
        return false;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    struct timeval tv;
    tv.tv_sec  = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    const std::string msg = "LEGO_DISCOVER";

    Logger::log("INTERMEDIARY", "SERVER_DISCOVERY", "buscando mi servidor por UDP " + std::to_string(PORT_SERVER_DISC) + " (broadcast " + broadcastAddr + ")...");

    while(true){

        // broadcast entre maquinas en la subred
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port   = htons(PORT_SERVER_DISC);

        if(inet_pton(AF_INET, broadcastAddr.c_str(), &dest.sin_addr) > 0) {

            sendto(sockfd, msg.c_str(), msg.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
        }

        // unicast a loopback -> por si el broadcast no rota en un solo host
        struct sockaddr_in lo;
        memset(&lo, 0, sizeof(lo));
        lo.sin_family = AF_INET;
        lo.sin_port   = htons(PORT_SERVER_DISC);
        inet_pton(AF_INET, "127.0.0.1", &lo.sin_addr);
        sendto(sockfd, msg.c_str(), msg.size(), 0, (struct sockaddr*)&lo, sizeof(lo));

        // esperar respuesta hasta el timeout arbitrario de 2s porque quiero jeje ATT: Juan
        char buf[128];
        struct sockaddr_in from;
        socklen_t slen = sizeof(from);
        memset(buf, 0, sizeof(buf));

        ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &slen);

        if(n > 0){

            buf[n] = '\0';
            int p;

            if(sscanf(buf, "LEGO_SERVER %d", &p) == 1) {

                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));
                figureServerIp = std::string(ipStr);
                figureServerPort = p;
                close(sockfd);

                Logger::log("INTERMEDIARY", "SERVER_DISCOVERY", "Servidor encontrado: " + figureServerIp + ":" + std::to_string(figureServerPort));
                return true;
            }
        }

        // timeout o paquete invalido -> se debe REINTENTAR
    }
}

// fetchFigureList() -> envia P/R/dir al servidor y devuelve la lista de nombres
std::vector<std::string> IntermediaryServer::fetchFigureList(const std::string & ip, int port) {

    std::string response = sendProtoMessage(ip, port, "P/R/dir\n");

    // formato de respuesta esperado: P/D/fig1,fig2,fig3
    std::vector<std::string> figures;

    // buscar el tercer campo despues de P/D/
    if(response.size() < 5 || response.substr(0, 4) != "P/D/") {

        Logger::log("INTERMEDIARY", "ERROR", "Respuesta inesperada de P/R/dir: " + response);
        return figures;
    }

    std::string data = response.substr(4);

    // eliminar \n y \r al final
    while(!data.empty() && (data.back() == '\n' || data.back() == '\r')) {

        data.pop_back();
    }

    // si data es 404 o esta vacia, es porque no hay figuras
    if(data.empty() || data == "404") {

        return figures;
    }

    // separar por comas
    std::istringstream ss(data);
    std::string token;
    while(std::getline(ss, token, ',')) {

        if(!token.empty()) {

            figures.push_back(token);
        }
    }

    return figures;
}

// abre conexion TCP, envia un mensaje de protocolo Pt y devuelve
std::string IntermediaryServer::sendProtoMessage(const std::string & ip, int port, const std::string & msg) {

    try {

        Socket s('s', false);
        s.Connect(ip.c_str(), port);
        s.Write(msg.c_str());

        char buffer[4096];
        std::string response;
        size_t bytesRead;

        while((bytesRead = s.Read(buffer, sizeof(buffer) - 1)) > 0) {

            buffer[bytesRead] = '\0';
            response += buffer;
            // el server cierra tras responder
        }

        s.Close();
        return response;

    } catch(const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("sendProtoMessage error: ") + e.what());

        return "";
    }
}

// consulta P/G/<figura>:<parte> al servidor correspondiente y devuelve el campo de datos (lista de piezas) del protocolo Pt -> half: 1 = primera mitad, 2 = segunda mitad, 0 = ambas
std::string IntermediaryServer::queryFigureServer(const std::string & ip, int port, const std::string & figureName, uint8_t half) {

    std::string protoMsg;

    if(half == 0) {

        protoMsg = "P/G/" + figureName + "\n";

    } else {

        protoMsg = "P/G/" + figureName + ":" + std::to_string(half) + "\n";
    }

    Logger::log("INTERMEDIARY", "PROTO_REQUEST", "Enviando: " + protoMsg.substr(0, protoMsg.size()-1) + " -> " + ip + ":" + std::to_string(port));

    std::string response = sendProtoMessage(ip, port, protoMsg);

    // formato: P/D/<datos>
    if(response.size() < 4 || response.substr(0, 4) != "P/D/") {

        Logger::log("INTERMEDIARY", "ERROR", "Respuesta inesperada de P/G: " + response);
        return "";
    }

    std::string data = response.substr(4);
    while(!data.empty() && (data.back() == '\n' || data.back() == '\r')) {

        data.pop_back();
    }

    // 404 = figura no encontrada
    if(data == "404" || data == "400") {

        return "";
    }

    return data;
}

// imprime la tabla de rutas en consola y log
void IntermediaryServer::printRouteTable() const {

    std::cout << "\n========= TABLA DE RUTAS =========\n";
    std::cout << std::left
              << std::setw(20) << "Figura"
              << std::setw(16) << "IP Servidor"
              << "Puerto\n";

    for(const auto & entry : routeTable) {

        const FigureRoute & r = entry.second;

        std::cout << std::left
                  << std::setw(20) << r.figureName
                  << std::setw(16) << r.serverIp
                  << r.serverPort << "\n";

        Logger::log("INTERMEDIARY", "ROUTE", "figura=" + r.figureName + " ip=" + r.serverIp + " puerto=" + std::to_string(r.serverPort));
    }
}

// loop principal -> acepta conexiones de clientes HTTP NachOS
void IntermediaryServer::listenClients() {

    Socket serverSock('s', false);
    serverSock.Bind("0.0.0.0", clientPort);
    serverSock.MarkPassive(20);

    std::cout << "Intermediario escuchando clientes en 0.0.0.0:" << clientPort << std::endl;

    Logger::log("INTERMEDIARY", "LISTEN", "Clientes en 0.0.0.0:" + std::to_string(clientPort));

    while(true) {

        VSocket* client = serverSock.AcceptConnection();

        std::thread t([this, client]() { handleClient(client); });
        t.detach();
    }
}

// atiende un cliente individual
void IntermediaryServer::handleClient(VSocket* client ) {

    try {

        std::string request = readRequest(client);
        
        if(request.empty()) {
        
            client->Close();
            delete client;
            return;
        }

        Logger::log("INTERMEDIARY", "CLIENT_REQUEST", request.substr(0, request.find('\n')));

        std::string response = processHttpRequest(request);
        client->Write(response.c_str());
        client->Shutdown(SHUT_WR);

    } catch (const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("handleClient: ") + e.what());

        try {

            std::string body = "<html><body><h1>500 Internal Server Error</h1><p>" + std::string(e.what()) + "</p></body></html>";

            client->Write(buildHttpResponse(body, "500 Internal Server Error").c_str());

        } catch (...) {

        }
    }

    try {

        client->Close();
    } catch (...) {

    }

    delete client;
}

// ;ee la solicitud completa del socket.
std::string IntermediaryServer::readRequest( VSocket * client ){
    std::string request;
    char buffer[2048];
    size_t bytesRead;

    while((bytesRead = client->Read(buffer, sizeof(buffer) - 1)) > 0){
        buffer[bytesRead] = '\0';
        request += buffer;

        if(request.find("\r\n\r\n") != std::string::npos) {

            break;
        }
    }

    return request;
}

// eutea la solicitud HTTP segun la ruta
std::string IntermediaryServer::processHttpRequest(const std::string & request) {

    bool nachos = isNachosRequest(request);
    std::string path;

    try{
        path = getPathFromRequest(request);
    }   
        catch (...)
    {
        return buildHttpResponse("<html><body><h1>400 Bad Request</h1></body></html>", "400 Bad Request");
    }

    if(path == "/lego/index.php" || path == "/") {

        return buildHttpResponse(handleIndex(nachos));
    }

    if(path.find("/lego/list.php") == 0) {

        return buildHttpResponse(handleList(path, nachos));
    }

    // estas rutas son las que me reenvia otro intermediario
    if(path == "/list") {
        
        return handleHttpList();
    }
    if(path.find("/figura/") == 0) {

        return handleHttpFigure(path);
    }

    return buildHttpResponse("<html><body><h1>404 Not Found</h1></body></html>", "404 Not Found");
}

// devuelve la lista de figuras disponibles.
std::string IntermediaryServer::handleIndex(bool nachos) {

    std::lock_guard<std::mutex> lock(routeTableMutex);

    Logger::log("INTERMEDIARY", "HANDLE_INDEX", "figuras disponibles: " + std::to_string(routeTable.size()));

    if(nachos) {

        std::string result;

        for(const auto & entry : routeTable) {

            result += entry.first + "\n";
        }

        return result;
    }

    // respuesta HTML
    std::ostringstream html;
    html << "<html><head><meta charset=\"UTF-8\"><title>Intermediario Lego</title>"
         << "<style>"
         << "body{font-family:Arial,sans-serif;margin:30px;background:#f0f4f8;color:#222;}"
         << "h1{color:#1a237e;} h2{color:#555;font-weight:normal;}"
         << ".card{background:white;padding:25px;border-radius:12px;"
         << "box-shadow:0 2px 8px rgba(0,0,0,.12);margin-bottom:20px;}"
         << "table{border-collapse:collapse;width:100%;margin-top:15px;}"
         << "th{background:#1a237e;color:white;padding:10px;text-align:left;}"
         << "td{border:1px solid #ddd;padding:10px;}"
         << "tr:nth-child(even){background:#f5f5f5;}"
         << ".badge{background:#e8eaf6;color:#1a237e;padding:3px 9px;"
         << "border-radius:12px;font-size:13px;font-weight:bold;}"
         << "a{color:#1a237e;text-decoration:none;font-weight:bold;}"
         << "a:hover{text-decoration:underline;}"
         << "</style></head><body>"
         << "<div class=\"card\">"
         << "<h1>Servidor Intermediario - Lego</h1>"
         << "<h2>Tabla de rutas: " << routeTable.size() << " figura(s) registrada(s)</h2>"
         << "<table>"
         << "<tr><th>Figura</th><th>Servidor IP</th><th>Puerto</th>"
         << "<th>Parte 1</th><th>Parte 2</th></tr>";

    for(const auto & entry : routeTable) {

        const FigureRoute & r = entry.second;

        html << "<tr>"
             << "<td><span class=\"badge\">" << r.figureName << "</span></td>"
             << "<td>" << r.serverIp << "</td>"
             << "<td>" << r.serverPort << "</td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.figureName << "&part=1\">Ver parte 1</a></td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.figureName << "&part=2\">Ver parte 2</a></td>"
             << "</tr>";
    }

    html << "</table></div></body></html>";
    return html.str();
}

// busca las piezas de una figura. puede estar en mi server, en un peer binario o en un remoto http
std::string IntermediaryServer::handleList(const std::string & path, bool nachos) {

    std::string figure = getQueryParam(path, "figure");
    std::string partStr = getQueryParam(path, "part");

    uint8_t half = (partStr == "2") ? 2 : 1;

    Logger::log("INTERMEDIARY", "HANDLE_LIST", "figure=" + figure + " part=" + partStr);

    std::string protoData;
    bool resolved = false;

    // primero en la tabla locales y remotas que ya conozco
    std::string targetIp;
    int targetPort = 0;
    RouteKind kind = RouteKind::LOCAL_PT;
    bool inTable = false;
    {
        std::lock_guard<std::mutex> lock(routeTableMutex);
        auto it = routeTable.find(figure);

        if(it != routeTable.end()) {

            targetIp = it->second.serverIp;
            targetPort = it->second.serverPort;
            kind = it->second.kind;
            inTable = true;
        }
    }

    if(inTable) {

        if(kind == RouteKind::REMOTE_HTTP) {

            protoData = queryRemoteIntermediary(targetIp, targetPort, figure, half);
        } else {

            protoData = queryFigureServer(targetIp, targetPort, figure, half);
        }

        if(!protoData.empty()) {
            resolved = true;
        }
    }

    // en los peers binarios

    if(!resolved) {

        std::string pIp;
        bool inPeers = false;
        {
            std::lock_guard<std::mutex> plock(peersMutex);
            for(const auto & peer : peers) {

                for(const auto & pf : peer.figures) {

                    if(pf == figure) {
                        pIp = peer.ip; 
                        inPeers = true; 
                        break;
                    }
                }
                if(inPeers) {
                    break;
                }
            }
        }

        if(inPeers) {

            Logger::log("INTERMEDIARY", "PEER_ROUTE", "figura=" + figure + " encontrada en peer binario=" + pIp);

            protoData = queryFigureServer(pIp, PORT_PEER_TCP, figure, half);
            
            if(!protoData.empty()) {
                resolved = true;
            }
        }
    }

    // le pregunto en vivo a los remotos que conozco
    if(!resolved) {

        std::vector<HttpPeer> hpCopy;
        { std::lock_guard<std::mutex> hlock(httpPeersMutex); hpCopy = httpPeers; }
        for(const auto & hp : hpCopy) {

            Logger::log("INTERMEDIARY", "REMOTE_FALLBACK", "Probando figura=" + figure + " en " + hp.ip + ":" + std::to_string(hp.port));
            protoData = queryRemoteIntermediary(hp.ip, hp.port, figure, half);
            
            if(!protoData.empty()) {
                resolved = true; 
                break;
            }
        }
    }

    if(!resolved) {

        Logger::log("INTERMEDIARY", "FIGURE_NOT_FOUND", "figure=" + figure + " part=" + partStr);

        if(nachos) {
            return "FIGURE_NOT_FOUND\n";
        }

        return "<html><body><h1>Figura no encontrada</h1>"
               "<p>La figura <b>" + figure + "</b> no existe en ningún servidor registrado.</p>"
               "</body></html>";
    }

    Logger::log("INTERMEDIARY", "FIGURE_FOUND", "figure=" + figure + " part=" + partStr);

    if(nachos) {

        return protoDataToNachos(protoData);
    }

    return protoDataToHtml(protoData, figure, partStr);
}

// convierte "qty|desc,qty|desc" -> "qty|desc\nqty|desc\n"
// formato que espera legoclient.c
std::string IntermediaryServer::protoDataToNachos(const std::string & protoData) {

    std::string result;
    std::istringstream ss(protoData);
    std::string token;

    while(std::getline(ss, token, ',')) {

        if(!token.empty()) {

            result += token + "\n";
        }   
    }

    return result;
}

// convierte "qty|desc,qty|desc" -> tabla HTML
std::string IntermediaryServer::protoDataToHtml(const std::string & protoData, const std::string & figure, const std::string & part ) {

    std::ostringstream html;
    html << "<html><head><meta charset=\"UTF-8\"><title>Piezas - " << figure << "</title>"
         << "<style>"
         << "body{font-family:Arial,sans-serif;margin:30px;background:#f0f4f8;color:#222;}"
         << "h1{color:#1a237e;} h2{color:#555;font-weight:normal;}"
         << ".card{background:white;padding:25px;border-radius:12px;"
         << "box-shadow:0 2px 8px rgba(0,0,0,.12);}"
         << "table{border-collapse:collapse;width:100%;margin-top:15px;}"
         << "th{background:#1a237e;color:white;padding:10px;text-align:left;}"
         << "td{border:1px solid #ddd;padding:10px;}"
         << "tr:nth-child(even){background:#f5f5f5;}"
         << ".total{background:#e8eaf6;font-weight:bold;}"
         << "a{color:#1a237e;}"
         << "</style></head><body>"
         << "<div class=\"card\">"
         << "<h1>Piezas: " << figure << "</h1>"
         << "<h2>Parte: " << part << "</h2>"
         << "<p><a href=\"/lego/index.php\">&larr; Volver al listado</a></p>"
         << "<table>"
         << "<tr><th>Cantidad</th><th>Descripcion</th></tr>";

    int total = 0;
    std::istringstream ss(protoData);
    std::string token;

    while(std::getline(ss, token, ',')) {

        size_t pipe = token.find('|');

        if(pipe == std::string::npos) {
            continue;
        }

        try {
            int qty = std::stoi(token.substr(0, pipe));
            std::string desc = token.substr(pipe + 1);
            total += qty;

            html << "<tr><td>" << qty << "</td><td>" << desc << "</td></tr>";

        } catch (...) {
            continue;
        }
    }

    html << "<tr class=\"total\"><td colspan=\"2\">Total: " << total << " piezas</td></tr>";
    html << "</table></div></body></html>";

    return html.str();
}

// construye una respuesta HTTP
std::string IntermediaryServer::buildHttpResponse(const std::string & body, const std::string & status, const std::string & ctype) {

    std::ostringstream r;
    r << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << ctype << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;
    return r.str();
}

// intermediarios remotods HTTP

// saca el codigo de estado de la primera linea HTTP -> o -1
int IntermediaryServer::extractHttpStatus(const std::string & resp) const {

    size_t sp = resp.find(' ');

    if(sp == std::string::npos) {
        return -1;
    }

    size_t sp2 = resp.find(' ', sp + 1);
    
    std::string code;

    if(sp2 == std::string::npos) {

        code = resp.substr(sp + 1);
    } else {
        
        code = resp.substr(sp + 1, sp2 - sp - 1);
    }

    try { 

        return std::stoi(code); 
    } catch (...) {
        
        return -1; 
    }
}

// devuelve lo que va despues de la cabecera o sea el cuerpo
std::string IntermediaryServer::extractHttpBody(const std::string & resp) const {

    size_t pos = resp.find("\r\n\r\n");

    if(pos != std::string::npos) {

        return resp.substr(pos + 4);
    }

    pos = resp.find("\n\n");

    if(pos != std::string::npos) {

        return resp.substr(pos + 2);
    }

    return resp;
}

// el server remoto manda  figura=.. / mitad=.. / pieza=cant / total=..
std::string IntermediaryServer::httpBodyToProtoData(const std::string & httpResponse) {

    std::string body = extractHttpBody(httpResponse);
    std::string result;
    std::istringstream ss(body);
    std::string line;

    while(std::getline(ss, line, '\n')) {

        if(!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if(line.empty()) {
            continue;
        }

        size_t eq = line.find('=');

        if(eq == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eq); // nombre de la pieza
        std::string val = line.substr(eq + 1); // cantidad

        if(key == "figura" || key == "mitad" || key == "total") {
            continue;
        }

        if(!result.empty()) {
            result += ",";
        }

        result += val + "|" + key;
    }

    return result;
}

// lo inverso: de mi formato qty|desc al cuerpo que entiende el server remoto
std::string IntermediaryServer::protoDataToHttpBody(const std::string & protoData, const std::string & figureName, uint8_t half ) {

    std::ostringstream body;

    body << "figura=" << figureName << "\n";
    body << "mitad="  << (int)half  << "\n";

    int total = 0;
    std::istringstream ss(protoData);
    std::string token;

    while(std::getline(ss, token, ',')) {

        size_t pipe = token.find('|');

        if(pipe == std::string::npos) {
            continue;
        }

        std::string qty  = token.substr(0, pipe);
        std::string desc = token.substr(pipe + 1);
        body << desc << "=" << qty << "\n";

        try {

            total += std::stoi(qty);
        } catch (...) {

        }
    }

    body << "total=" << total << "\n";
    return body.str();
}

// le pido una figura a un intermediario remoto y traduzco su respuesta
std::string IntermediaryServer::queryRemoteIntermediary(const std::string & ip, int port, const std::string & figureName, uint8_t half ) {

    std::string req =
        "GET /figura/" + figureName + "/" + std::to_string((int)half) + " HTTP/1.1\r\n"
        "Host: " + ip + "\r\n"
        "Via: intermediario\r\n"
        "Connection: close\r\n"
        "\r\n";

    Logger::log("INTERMEDIARY", "REMOTE_REQUEST", "GET /figura/" + figureName + "/" + std::to_string((int)half) + " -> " + ip + ":" + std::to_string(port));

    std::string resp = sendProtoMessage(ip, port, req);

    if(resp.empty()) {
        return "";
    }

    int status = extractHttpStatus(resp);

    if(status != 200) {

        Logger::log("INTERMEDIARY", "REMOTE_MISS", "figura=" + figureName + " status=" + std::to_string(status));
        return "";

    }

    return httpBodyToProtoData(resp);
}

// le pido la lista GET /list a un intermediario remoto
std::vector<std::string> IntermediaryServer::fetchRemoteFigureList(const std::string & ip, int port) {

    std::vector<std::string> figures;
    std::string req =
        "GET /list HTTP/1.1\r\n"
        "Host: " + ip + "\r\n"
        "Via: intermediario\r\n"
        "Connection: close\r\n"
        "\r\n";

    std::string resp = sendProtoMessage(ip, port, req);

    if(resp.empty() || extractHttpStatus(resp) != 200) {
        return figures;
    }

    std::string body = extractHttpBody(resp);
    std::istringstream ss(body);
    std::string line;

    while(std::getline(ss, line, '\n')) {

        if(!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if(!line.empty()) {
            figures.push_back(line);
        }
    }

    return figures;
}

// cuando otro intermediario me pide /list, le devuelvo mis figuras locales
std::string IntermediaryServer::handleHttpList() {

    std::string body;
    {
        std::lock_guard<std::mutex> lock(routeTableMutex);
        for(const auto & entry : routeTable) {

            if(entry.second.kind == RouteKind::LOCAL_PT) {

                body += entry.first + "\n";
            }
        }
    }

    Logger::log("INTERMEDIARY", "HTTP_LIST", "Lista local enviada a cliente remoto");

    return buildHttpResponse(body, "200 OK", "text/plain");
}

// otro intermediario me pide una figura -> la busco en mi server y se la devuelvo en su formato
std::string IntermediaryServer::handleHttpFigure(const std::string & path) {

    // path: /figura/<nombre>/<mitad>
    std::string rest = path.substr(std::string("/figura/").size());
    size_t q = rest.find('?');

    if(q != std::string::npos) {

        rest = rest.substr(0, q);
    }

    size_t slash = rest.find('/');

    if(slash == std::string::npos) {

        return buildHttpResponse("Solicitud invalida.\n", "400 Bad Request", "text/plain");
    }

    std::string name = rest.substr(0, slash);
    std::string mitadStr = rest.substr(slash + 1);
    uint8_t half = (mitadStr == "2") ? 2 : 1;

    Logger::log("INTERMEDIARY", "HTTP_FIGURE", "Cliente remoto pide figura=" + name + " mitad=" + mitadStr);

    // solo lo sirvo desde mi server asi no se arma un bucle entre intermediarios
    std::string ip; int port = 0; bool localFound = false; {

        std::lock_guard<std::mutex> lock(routeTableMutex);
        auto it = routeTable.find(name);

        if(it != routeTable.end() && it->second.kind == RouteKind::LOCAL_PT) {

            ip = it->second.serverIp; port = it->second.serverPort; localFound = true;
        }
    }

    if(!localFound) {

        return buildHttpResponse("Error: figura no encontrada.\n", "404 Not Found", "text/plain");
    }

    std::string protoData = queryFigureServer(ip, port, name, half);

    if(protoData.empty()) {

        return buildHttpResponse("Error: figura no encontrada.\n", "404 Not Found", "text/plain");
    }

    std::string body = protoDataToHttpBody(protoData, name, half);
    return buildHttpResponse(body, "200 OK", "text/plain");
}

// guarda un intermediario remoto y mete sus figuras en mi tabla
void IntermediaryServer::addHttpPeerAndMerge(const std::string & ip, int port) {
    
    bool alreadyMerged = false;
    {
        std::lock_guard<std::mutex> lock(httpPeersMutex);
        bool known = false;

        for(const auto & p : httpPeers) {

            if(p.ip == ip && p.port == port) {
                known = true; alreadyMerged = p.merged; break;
            }
        }

        if(!known) {

            httpPeers.push_back(HttpPeer{ ip, port, false });
            Logger::log("INTERMEDIARY", "HTTP_PEER_ADDED", ip + ":" + std::to_string(port));
        }
    }

    if(alreadyMerged) {
        return;
    }

    std::vector<std::string> figs = fetchRemoteFigureList(ip, port);

    if(figs.empty()) {
        return; // todavia no responde -> se reintenta despues
    }

    int added = 0;
    {
        std::lock_guard<std::mutex> lock(routeTableMutex);
        for (const auto & f : figs) {

            if(routeTable.find(f) == routeTable.end()) {

                routeTable[f] = FigureRoute{ f, ip, port, RouteKind::REMOTE_HTTP };
                ++added;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(httpPeersMutex);

        for(auto & p : httpPeers) {

            if(p.ip == ip && p.port == port) {
                p.merged = true; break;
            }
        }
    }

    Logger::log("INTERMEDIARY", "HTTP_PEER_MERGED", ip + ":" + std::to_string(port) + " figuras_nuevas=" + std::to_string(added));
}

// aviso por UDP que existo INTER_HERE para que otros intermediarios me encuentren
void IntermediaryServer::announceToPeers() {

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "announceToPeers: no se pudo crear socket UDP");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    std::string msg = "INTER_HERE " + std::to_string(clientPort);

    Logger::log("INTERMEDIARY", "PEER_ANNOUNCE", "Anunciando '" + msg + "' a " + broadcastAddr + ":" + std::to_string(PORT_REMOTE_DISC));

    while(true) {

        // broadcast a la subred
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(PORT_REMOTE_DISC);

        if(inet_pton(AF_INET, broadcastAddr.c_str(), &dest.sin_addr) > 0) {

            sendto(sockfd, msg.c_str(), msg.size(), 0, (struct sockaddr*)&dest, sizeof(dest));
        }

        // y unicast a los que ya conozco sirve cuando todo corre en un mismo equipo
        std::vector<HttpPeer> hpCopy;
        { std::lock_guard<std::mutex> lock(httpPeersMutex); hpCopy = httpPeers; }
        
        for(const auto & hp : hpCopy) {

            struct sockaddr_in u;
            memset(&u, 0, sizeof(u));
            u.sin_family = AF_INET;
            u.sin_port = htons(PORT_REMOTE_DISC);

            if(inet_pton(AF_INET, hp.ip.c_str(), &u.sin_addr) > 0) {

                sendto(sockfd, msg.c_str(), msg.size(), 0, (struct sockaddr*)&u, sizeof(u));
            }
        }

        // aprovecho para reintentar la fusion si es necesario
        for(const auto & hp : hpCopy) {

            addHttpPeerAndMerge(hp.ip, hp.port);
        }

        sleep(5);
    }
}

// escucha los INTER_HERE de otros intermediarios remotos
void IntermediaryServer::listenHttpPeers() {

    // para el otro intermediario con la semilla y el anuncio unicast alcanza
    if(!seedPeerIp.empty()) {

        Logger::log("INTERMEDIARY", "PEER_DISCOVERY", "Modo local (semilla): no se ocupa UDP " + std::to_string(PORT_REMOTE_DISC) + "; salida por semilla, entrada por unicast.");
        return;
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "listenHttpPeers: no se pudo crear socket UDP");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


    // a proposito sin SO_REUSEPORT si el puerto ya esta ocupado prefiero que falle el bind

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT_REMOTE_DISC);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {

        // no se pudo porque seguro hay otro intermediario en el mismo equipo
        Logger::log("INTERMEDIARY", "WARN", "listenHttpPeers: no se pudo bind UDP " + std::to_string(PORT_REMOTE_DISC) + " (¿otro intermediario en el mismo host?). Descubrimiento entrante deshabilitado.");
        close(sockfd);
        return;
    }

    Logger::log("INTERMEDIARY", "LISTEN", "UDP descubrimiento de intermediarios remotos en puerto " + std::to_string(PORT_REMOTE_DISC));

    while(true) {

        char buf[128];
        struct sockaddr_in from;
        socklen_t slen = sizeof(from);
        memset(buf, 0, sizeof(buf));

        ssize_t n = recvfrom(sockfd, buf, sizeof(buf) - 1, 0, (struct sockaddr*)&from, &slen);
        
        if(n <= 0) {
            continue;
        }

        buf[n] = '\0';

        int peerPort;

        if(sscanf(buf, "INTER_HERE %d", &peerPort) != 1) {
            continue;
        }

        // ignorar nuestro propio anuncio porque es mismo puerto que clientPort
        if(peerPort == clientPort) {
            continue;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));

        Logger::log("INTERMEDIARY", "PEER_DISCOVERED", std::string(ipStr) + ":" + std::to_string(peerPort));
        addHttpPeerAndMerge(std::string(ipStr), peerPort);
    }
}

// obtiene la ruta de una solicitud HTTP
std::string IntermediaryServer::getPathFromRequest(const std::string & request) {

    std::istringstream s(request);
    std::string method, path, version;
    s >> method >> path >> version;

    if(method != "GET") {

        throw std::runtime_error("metodo HTTP no soportado");
    }

    return path;
}

// obtiene un parámetro de la ruta
std::string IntermediaryServer::getQueryParam(const std::string & path, const std::string & key) {

    size_t q = path.find('?');

    if(q == std::string::npos) {

        return "";
    }

    std::string query = path.substr(q + 1);
    std::string pattern = key + "=";

    size_t start = query.find(pattern);

    if(start == std::string::npos) {

        return "";
    }

    start += pattern.size();
    size_t end = query.find('&', start);

    if(end == std::string::npos) {

        return query.substr(start);
    }

    return query.substr(start, end - start);
}

// detecta si es una solicitud de NachOS
bool IntermediaryServer::isNachosRequest(const std::string & request) const {

    return request.find("User-Agent: nachos") != std::string::npos;
}


// PROTOCOLO INTER-INTERMEDIARIO

// escucha paquetes JOIN de otros intermediarios por UDP y cuando recibe uno lanza un hilo para hacer HANDSHAKE TCP
void IntermediaryServer::listenJoinUdp() {

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "No se pudo crear socket UDP para JOIN");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT_JOIN_UDP);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "No se pudo bind UDP " + std::to_string(PORT_JOIN_UDP));
        close(sockfd);
        return;
    }

    Logger::log("INTERMEDIARY", "LISTEN", "UDP JOIN en puerto " + std::to_string(PORT_JOIN_UDP));
    std::cout << "Intermediario escuchando JOIN UDP en puerto " << PORT_JOIN_UDP << "\n";

    // formato del paquete JOIN segun protocolo -> uint8_t tipo = 0 JOIN -> in_addr sourceIp 4 bytes
    while(true) {
        uint8_t pkt[6];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);

        ssize_t n = recvfrom(sockfd, pkt, sizeof(pkt), 0, (struct sockaddr*)&sender, &slen);

        if(n < 5) {

            continue;
        }

        if(pkt[0] != PKT_JOIN) {

            continue;
        }

        // extraer IP del campo sourceIp
        struct in_addr srcAddr;
        memcpy(&srcAddr, &pkt[1], sizeof(srcAddr));

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &srcAddr, ipStr, sizeof(ipStr));

        std::string peerIp(ipStr);

        // no hacer handshake consigo mismo
        if(peerIp == bindIp || peerIp == "127.0.0.1") {

            continue;
        }

        // verificar si ya conocemos este peer
        {
            std::lock_guard<std::mutex> lock(peersMutex);
            bool yaConocido = false;

            for(const auto & p : peers) {

                if(p.ip == peerIp) {

                    yaConocido = true;
                    break;
                }
            }

            if(yaConocido) {
                continue;
            }
        }

        Logger::log("INTERMEDIARY", "JOIN_RECEIVED", "Nuevo intermediario: " + peerIp);

        // lanzar hilo para hacer HANDSHAKE TCP al peer
        std::thread t([this, peerIp]() {

            try {
                Socket tcpSock('s', false);
                tcpSock.Connect(peerIp.c_str(), PORT_PEER_TCP);

                // enviar HANDSHAKE
                std::string payload = buildHandshakePayload();

                // formato -> uint8_t tipo | uint32_t len | char[n]
                uint8_t tipo = PKT_HANDSHAKE;
                uint32_t len = htonl((uint32_t)payload.size());

                tcpSock.Write(&tipo, 1);
                tcpSock.Write(&len, 4);
                
                if(!payload.empty()) {

                    tcpSock.Write(payload.c_str());
                }

                // leer HANDSHAKE del peer
                char hdrBuf[5];
                size_t rd = tcpSock.Read(hdrBuf, 5);

                if(rd >= 5 && (uint8_t)hdrBuf[0] == PKT_HANDSHAKE) {

                    uint32_t plen;
                    memcpy(&plen, &hdrBuf[1], 4);
                    plen = ntohl(plen);

                    std::string peerPayload(plen, '\0');
                    if(plen > 0) {

                        tcpSock.Read(&peerPayload[0], plen);
                    }

                    parseHandshake(peerPayload, peerIp);
                }

                tcpSock.Close();
                Logger::log("INTERMEDIARY", "HANDSHAKE_DONE", "Con peer=" + peerIp);
            }
            catch(const std::exception & e) {

                Logger::log("INTERMEDIARY", "ERROR", std::string("HANDSHAKE fallo con ") + peerIp + ": " + e.what());
            }
        });

        t.detach();
    }
}


// anuncia periodicamente este intermediario a otros enviando paquetes JOIN por UDP broadcast
void IntermediaryServer::broadcastJoinUdp() {

    // direcciones de broadcast para cada isla de la red
    const char* broadcasts[] = {
        // "172.16.123.15",  // isla 1
        // "172.16.123.31",  // isla 2
        // "172.16.123.47",  // isla 3
        // "172.16.123.63",  // isla 4
        // "172.16.123.79",  // isla 5
        // "172.16.123.95",  // isla 6
        "255.255.255.255",   // broadcast general (prueba local)
    };

    const int numBroadcasts = sizeof(broadcasts) / sizeof(broadcasts[0]);

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "No se pudo crear socket UDP para broadcast JOIN");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // construir paquete JOIN: tipo(1) + sourceIp(4)
    uint8_t pkt[5];
    pkt[0] = PKT_JOIN;

    // usar la IP de bind como sourceIp
    struct in_addr myAddr;

    if(bindIp == "0.0.0.0" || inet_pton(AF_INET, bindIp.c_str(), &myAddr) <= 0) {

        // si bindIp es 0.0.0.0, usar 127.0.0.1 como fallback
        inet_pton(AF_INET, "127.0.0.1", &myAddr);
    }

    memcpy(&pkt[1], &myAddr, sizeof(myAddr));

    Logger::log("INTERMEDIARY", "BROADCAST", "Iniciando broadcast JOIN periodico");

    while(true) {

        for(int i = 0; i < numBroadcasts; ++i) {

            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT_JOIN_UDP);
            inet_pton(AF_INET, broadcasts[i], &dest.sin_addr);

            sendto(sockfd, pkt, sizeof(pkt), 0, (struct sockaddr*)&dest, sizeof(dest));
        }

        sleep(5);
    }
}

// escucha en TCP PORT_PEER_TCP -> paquetes HANDSHAKE de peers que recibieron nuestro JOIN -> paquetes INTERMEDIARY_REQUEST
void IntermediaryServer::listenPeerTcp() {
    try {
        Socket serverSock('s', false);
        serverSock.Bind("0.0.0.0", PORT_PEER_TCP);
        serverSock.MarkPassive(10);

        Logger::log("INTERMEDIARY", "LISTEN", "TCP PEER en puerto " + std::to_string(PORT_PEER_TCP));
        std::cout << "Intermediario escuchando peers TCP en puerto " << PORT_PEER_TCP << "\n";

        while(true) {
            VSocket* peer = serverSock.AcceptConnection();
            std::thread t([this, peer]() { handleHandshake(peer); });
            t.detach();
        }

    } catch (const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("listenPeerTcp: ") + e.what());
    }
}


// lee el primer byte para determinar el tipo de paquete, si es HANDSHAKE intercambia figuras con el peer, y si es INTERMEDIARY_REQUEST procesa la solicitud de figura
void IntermediaryServer::handleHandshake(VSocket* peer ) {

    try {

        // obtener IP real del peer via getpeername
        std::string peerIp = getPeerIp(peer);

        uint8_t tipo;
        peer->Read(&tipo, 1);

        if(tipo == PKT_HANDSHAKE) {

            // leer longitud y contenido
            uint32_t netLen;
            peer->Read(&netLen, 4);
            uint32_t len = ntohl(netLen);

            std::string payload(len, '\0');
            
            if(len > 0) {
                peer->Read(&payload[0], len);
            }

            parseHandshake(payload, peerIp);

            // responder con nuestro HANDSHAKE
            std::string myPayload = buildHandshakePayload();
            uint8_t myTipo = PKT_HANDSHAKE;
            uint32_t myLen = htonl((uint32_t)myPayload.size());

            peer->Write(&myTipo, 1);
            peer->Write(&myLen, 4);

            if(!myPayload.empty()) {

                peer->Write(myPayload.c_str());
            }

            Logger::log("INTERMEDIARY", "HANDSHAKE_RECV", "Handshake completado. Figuras del peer: " + payload);

        } else if(tipo == PKT_IR_REQUEST) {

            // formato -> uint8_t half | uint8_t nameLen | char[n]
            uint8_t half, nameLen;
            peer->Read(&half, 1);
            peer->Read(&nameLen, 1);

            std::string figureName(nameLen, '\0');

            if(nameLen > 0) {

                peer->Read(&figureName[0], nameLen);
            }

            Logger::log("INTERMEDIARY", "IR_REQUEST", "figura=" + figureName + " half=" + std::to_string(half) + " from=" + peerIp);

            // buscar en tabla local
            std::string protoData;
            std::string targetIp;
            int targetPort = 0;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(routeTableMutex);
                auto it = routeTable.find(figureName);

                if(it != routeTable.end()) {

                    targetIp = it->second.serverIp;
                    targetPort = it->second.serverPort;
                    found = true;
                }
            }

            if(found) {

                protoData = queryFigureServer(targetIp, targetPort, figureName, half);
            }

            if(!protoData.empty()) {

                // enviar INTERMEDIARY_RESPONSE
                // tipo | half | figureNameLen | figureName | contentLen(u32) | content
                uint8_t rTipo = PKT_IR_RESPONSE;
                uint8_t rHalf = half;
                uint8_t rNameLen = (uint8_t)figureName.size();
                uint32_t rContentLen = htonl((uint32_t)protoData.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rHalf, 1);
                peer->Write(&rNameLen, 1);
                peer->Write(figureName.c_str());
                peer->Write(&rContentLen, 4);
                peer->Write(protoData.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE", "figura=" + figureName + " enviada");

            } else {

                // FIGURE_NOT_FOUND
                uint8_t rTipo = PKT_NOT_FOUND;
                uint32_t rContentLen = htonl((uint32_t)figureName.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rContentLen, 4);
                peer->Write(figureName.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE", "FIGURE_NOT_FOUND figura=" + figureName);
            }
        }
    } catch (const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("handleHandshake: ") + e.what());
    }

    try {

        peer->Close();
    } catch (...) {

    }

    delete peer;
}

// construye la lista de figuras locales separadas por coma
std::string IntermediaryServer::buildHandshakePayload() const {
    std::string payload;
    bool first = true;

    for(const auto & entry : routeTable) {

        if(!first) {

            payload += ",";
        }

        payload += entry.first;
        first = false;
    }

    return payload;
}

// parsea la lista de figuras recibida en un HANDSHAKE y registra el peer en la tabla de peers
void IntermediaryServer::parseHandshake(const std::string & payload, const std::string & peerIp ) {

    PeerInfo info;
    info.ip = peerIp;

    std::istringstream ss(payload);
    std::string token;

    while(std::getline(ss, token, ',')) {

        if(!token.empty()) {

            info.figures.push_back(token);
        }
    }

    {
        std::lock_guard<std::mutex> lock(peersMutex);

        // actualizar si ya existe el peer
        for(auto & p : peers) {

            if(p.ip == peerIp) {

                p.figures = info.figures;
                Logger::log("INTERMEDIARY", "PEER_UPDATED", "ip=" + peerIp + " figuras=" + std::to_string(info.figures.size()));
                return;
            }
        }

        peers.push_back(info);
    }

    Logger::log("INTERMEDIARY", "PEER_ADDED", "ip=" + peerIp + " figuras=" + std::to_string(info.figures.size()));
}

// IP del peer. sockId es protected en VSocket, asi que uso una subclase para sacar el fd
std::string IntermediaryServer::getPeerIp(VSocket* sock) const {

    struct sockaddr_in peerAddr;
    socklen_t addrLen = sizeof(peerAddr);
    memset(&peerAddr, 0, sizeof(peerAddr));

    int fd = -1;
    char ipStr[INET_ADDRSTRLEN] = "unknown";

    struct SockAccess : public VSocket {

        int getFd() const {
            return this->sockId;
        }

        // metodos vacios solo para poder instanciar
        int Connect(const char*, int) override {
            return 0;
        }

        int Connect(const char*, const char*) override {
            return 0;
        }

        size_t Read(void*, size_t) override {
            return 0;
        }

        size_t Write(const void*, size_t) override {
            return 0; 
        }

        size_t Write(const char*) override {
            return 0; 
        }

        VSocket* AcceptConnection() override { 
            return nullptr; 
        }

    };

    fd = reinterpret_cast<const SockAccess*>(sock)->getFd();

    if(fd >= 0) {

        if(getpeername(fd, (struct sockaddr*)&peerAddr, &addrLen) == 0) {

            inet_ntop(AF_INET, &peerAddr.sin_addr, ipStr, sizeof(ipStr));
        }
    }

    return std::string(ipStr);
}
