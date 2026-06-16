#include "IntermediaryServer.h"
#include "Logger.h"

#include <sstream>
#include <iostream>
#include <iomanip>
#include <thread>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>


IntermediaryServer::IntermediaryServer(const std::string & localIp, int servicePort, const std::string & backendIp, int backendPort) : localIp(localIp), servicePort(servicePort), backendIp(backendIp), backendPort(backendPort) {

}


void IntermediaryServer::start() {
    Logger::log("INTERMEDIARY", "START", "localIp=" + localIp + " servicePort=" + std::to_string(servicePort) + " backend=" + backendIp + ":" + std::to_string(backendPort));

    initRouteTable();
    displayRouteTable();

    std::thread udpThread([this](){receiveJoinUdp();});
    udpThread.detach();

    std::thread peerThread([this](){receivePeerTcp();});
    peerThread.detach();

    acceptClients();
}


void IntermediaryServer::initRouteTable()
{
    Logger::log("INTERMEDIARY", "ROUTE_TABLE", "construyendo la tabla de rutas...");

    std::vector<std::string>figures = requestFigureDirectory(backendIp, backendPort);

    std::lock_guard<std::mutex> lock(routingMutex);

    for(const auto & fig : figures) {

        RouteEntry entry;
        entry.name = fig;
        entry.address = backendIp;
        entry.portNumber = backendPort;
        routingTable[fig] = entry;
    }

    Logger::log("INTERMEDIARY", "ROUTE_TABLE", "figuras registradas: " + std::to_string(routingTable.size()));
}


std::vector<std::string> IntermediaryServer::requestFigureDirectory( const std::string & ip, int port ) {

    std::string response = dispatchProtoMessage(ip, port, "P/R/dir\n");
    std::vector<std::string> figures;

    if(response.size() < 5 || response.substr(0, 4) != "P/D/") {

        Logger::log("INTERMEDIARY", "ERROR", "respuesta inesperada de P/R/dir: " + response);
        return figures;
    }

    std::string data = response.substr(4);

    while(!data.empty() && (data.back() == '\n' || data.back() == '\r')) {

        data.pop_back();
    }

    if(data.empty() || data == "404") {
        return figures;
    }

    std::istringstream ss(data);
    std::string token;

    while(std::getline(ss, token, ',')) {

        if(!token.empty()) {

            figures.push_back(token);
        }
    }

    return figures;
}


std::string IntermediaryServer::dispatchProtoMessage(const std::string & ip, int port, const std::string & msg ) {
    
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
        }

        s.Close();
        return response;

    } catch(const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("dispatchProtoMessage error: ") + e.what());
        return "";
    }
}


std::string IntermediaryServer::fetchFigureData(const std::string & ip, int port, const std::string & figureName, uint8_t segment ) {

    std::string protoMsg;

    if(segment == 0) {

        protoMsg = "P/G/" + figureName + "\n";

    } else {
        protoMsg = "P/G/" + figureName + ":" + std::to_string(segment) + "\n";
    }

    Logger::log("INTERMEDIARY", "PROTO_REQUEST", "enviando: " + protoMsg.substr(0, protoMsg.size()-1) + " -> " + ip + ":" + std::to_string(port));

    std::string response = dispatchProtoMessage(ip, port, protoMsg);

    if(response.size() < 4 || response.substr(0, 4) != "P/D/") {

        Logger::log("INTERMEDIARY", "ERROR", "respuesta inesperada de P/G: " + response);
        return "";
    }

    std::string data = response.substr(4);
    while(!data.empty() && (data.back() == '\n' || data.back() == '\r')) {
        data.pop_back();
    }

    if(data == "404" || data == "400") {
        return "";
    }

    return data;
}


void IntermediaryServer::displayRouteTable() const {
    std::cout << "\nTABLA DE RUTAS\n";
    std::cout << std::left;
    std::cout << std::setw(20) << "Figura";
    std::cout << std::setw(16) << "IP Servidor";
    std::cout << "Puerto\n";

    for(const auto & entry : routingTable) {

        const RouteEntry & r = entry.second;
        
        std::cout << std::left;
        std::cout << std::setw(20) << r.name;
        std::cout << std::setw(16) << r.address;
        std::cout << r.portNumber << "\n";

        Logger::log("INTERMEDIARY", "ROUTE", "figura=" + r.name + " ip=" + r.address + " puerto=" + std::to_string(r.portNumber));
    }
}


void IntermediaryServer::acceptClients()
{
    Socket serverSock('s', false);
    serverSock.Bind(localIp.c_str(), servicePort);
    serverSock.MarkPassive(20);

    std::cout << "Intermediario escuchando clientes en " << localIp << ":" << servicePort << std::endl;
    
    Logger::log("INTERMEDIARY", "LISTEN", "clientes en " + localIp + ":" + std::to_string(servicePort));

    while(true) {

        VSocket * client = serverSock.AcceptConnection();
        std::thread t([this, client]() {serveClient(client);});
        t.detach();
    }
}


void IntermediaryServer::serveClient(VSocket* client ) {
    try {

        std::string request = receiveRequest(client);
        
        if(request.empty()) {
            client->Close();
            delete client;
            return;
        }

        Logger::log("INTERMEDIARY", "CLIENT_REQUEST", request.substr(0, request.find('\n')));

        std::string response = routeHttpRequest(request);
        client->Write(response.c_str());
        client->Shutdown(SHUT_WR);

    } catch(const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("serveClient: ") + e.what());
        
        try {
            std::string body = "<html><body><h1>500 internal Server Error</h1><p>" + std::string(e.what()) + "</p></body></html>";

            client->Write(composeHttpResponse(body, "500 internal Server Error").c_str());
        } catch (...) {

        }
    }

    try {

        client->Close();
    } catch (...) {

    }

    delete client;
}


std::string IntermediaryServer::receiveRequest(VSocket* client ) {

    std::string request;
    char buffer[2048];
    size_t bytesRead;

    while((bytesRead = client->Read(buffer, sizeof(buffer) - 1)) > 0) {

        buffer[bytesRead] = '\0';
        request += buffer;

        if(request.find("\r\n\r\n") != std::string::npos) {
            break;
        }
    }

    return request;
}


std::string IntermediaryServer::routeHttpRequest(const std::string & request) {

    bool nachos = detectNachos(request);
    std::string path;

    try {
        path = extractPath(request);

    } catch (...) {

        return composeHttpResponse("<html><body><h1>400 bbd request</h1></body></html>", "400 bad Request");
    }

    if(path == "/lego/index.php" || path == "/") {

        return composeHttpResponse(serveIndex(nachos));
    }

    if(path.find("/lego/list.php") == 0) {

        return composeHttpResponse(servePieceList(path, nachos));
    }

    return composeHttpResponse("<html><body><h1>404 Not Found</h1></body></html>", "404 not found");
}


std::string IntermediaryServer::serveIndex( bool nachos ) {

    std::lock_guard<std::mutex> lock(routingMutex);

    Logger::log("INTERMEDIARY", "HANDLE_INDEX", "figuras disponibles: " + std::to_string(routingTable.size()));

    if(nachos) {

        std::string result;
        
        for(const auto & entry : routingTable) {

            result += entry.first + "\n";
        }

        return result;
    }

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
         << "<h2>Tabla de rutas: " << routingTable.size() << " figura(s) registrada(s)</h2>"
         << "<table>"
         << "<tr><th>Figura</th><th>Servidor IP</th><th>Puerto</th>"
         << "<th>Parte 1</th><th>Parte 2</th></tr>";

    for (const auto & entry : routingTable) {

        const RouteEntry & r = entry.second;

        html << "<tr>"
             << "<td><span class=\"badge\">" << r.name << "</span></td>"
             << "<td>" << r.address << "</td>"
             << "<td>" << r.portNumber << "</td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.name << "&part=1\">Ver parte 1</a></td>"
             << "<td><a href=\"/lego/list.php?figure=" << r.name << "&part=2\">Ver parte 2</a></td>"
             << "</tr>";
    }

    html << "</table></div></body></html>";
    return html.str();
}


std::string IntermediaryServer::servePieceList(const std::string & path, bool nachos) {

    std::string figure = extractParam(path, "figure");
    std::string segStr = extractParam(path, "part");
    uint8_t segment = (segStr == "2") ? 2 : 1;

    Logger::log("INTERMEDIARY", "HANDLE_LIST", "figure=" + figure + " part=" + segStr);

    std::string targetIp;
    int targetPort = 0;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(routingMutex);
        auto it = routingTable.find(figure);
        
        if(it != routingTable.end()) {
            targetIp = it->second.address;
            targetPort = it->second.portNumber;
            found = true;
        }
    }

    if(!found) {

        Logger::log("INTERMEDIARY", "ROUTE_MISS", "figura=" + figure + " no en tabla local -> buscando en nodos...");

        std::lock_guard<std::mutex> nlock(nodesMutex);
        for(const auto & node : knownNodes) {

            for(const auto & nf : node.knownFigures) {

                if(nf == figure) {

                    targetIp = node.ipAddress;
                    targetPort = 3031;
                    found = true;
                    
                    Logger::log("INTERMEDIARY", "NODE_ROUTE", "figura=" + figure + " encontrada en nodo=" + node.ipAddress);
                    break;
                }
            }

            if(found) {
                break;
            }
        }
    }

    if(!found) {

        Logger::log("INTERMEDIARY", "FIGURE_NOT_FOUND", "figure=" + figure);

        if(nachos) {
            return "FIGURE_NOT_FOUND\n";
        }

        return "<html><body><h1>Figura no encontrada</h1>" "<p>la figura <b>" + figure + "</b> no existe en ningún servidor registrado.</p>" "</body></html>";
    }

    std::string protoData = fetchFigureData(targetIp, targetPort, figure, segment);

    if(protoData.empty()) {

        Logger::log("INTERMEDIARY", "FIGURE_NOT_FOUND", "figure=" + figure + " part=" + segStr + " sin datos");

        if(nachos) {
            return "FIGURE_NOT_FOUND\n";
        }

        return "<html><body><h1>Figura no encontrada</h1>" "<p>no se encontraron piezas para <b>" + figure + "</b> parte " + segStr + "</p>" "</body></html>";
    }

    Logger::log("INTERMEDIARY", "FIGURE_FOUND", "figure=" + figure + " part=" + segStr + " server=" + targetIp + ":" + std::to_string(targetPort));

    if(nachos) {

        return convertToPlainText(protoData);
    }

    return convertToHtmlTable(protoData, figure, segStr);
}


std::string IntermediaryServer::convertToPlainText(const std::string & protoData) {

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


std::string IntermediaryServer::convertToHtmlTable(const std::string & protoData, const std::string & figure, const std::string & segment ) {
    
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
         << "<h2>Parte: " << segment << "</h2>"
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

    html << "<tr class=\"total\"><td colspan=\"2\">total: " << total << " piezas</td></tr>";
    html << "</table></div></body></html>";

    return html.str();
}


std::string IntermediaryServer::composeHttpResponse(const std::string & body, const std::string & status, const std::string & ctype ) {

    std::ostringstream r;

    r << "HTTP/1.1 " << status << "\r\n"
      << "Content-Type: " << ctype << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "\r\n"
      << body;

    return r.str();
}


std::string IntermediaryServer::extractPath( const std::string & request ) {
    std::istringstream s(request);
    std::string method, path, version;
    s >> method >> path >> version;

    if(method != "GET") {

        throw std::runtime_error("metodo HTTP no soportado");
    }

    return path;
}


std::string IntermediaryServer::extractParam(const std::string & path, const std::string & key ) {

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

    std::string result;
    if(end == std::string::npos) {
        result = query.substr(start);
    } else {
        result = query.substr(start, end - start);
    }

    return result;
}


bool IntermediaryServer::detectNachos( const std::string & request ) const {
    return request.find("User-Agent: nachos") != std::string::npos;
}


void IntermediaryServer::receiveJoinUdp() {

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(sockfd < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "no se pudo crear socket UDP para JOIN");
        return;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(3030);

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {

        Logger::log("INTERMEDIARY", "ERROR", "no se pudo bind UDP 3030");
        close(sockfd);

        return;
    }

    Logger::log("INTERMEDIARY", "LISTEN", "UDP JOIN en puerto 3030");
    std::cout << "Intermediario escuchando JOIN UDP en puerto 3030\n";

    while(true) {

        uint8_t pkt[6];
        struct sockaddr_in sender;
        socklen_t slen = sizeof(sender);

        ssize_t n = recvfrom(sockfd, pkt, sizeof(pkt), 0, (struct sockaddr*)&sender, &slen);

        if(n < 5) {
            continue;
        }

        if(pkt[0] != MSG_JOIN) {
            continue;
        }

        struct in_addr srcAddr;
        memcpy(&srcAddr, &pkt[1], sizeof(srcAddr));

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &srcAddr, ipStr, sizeof(ipStr));
        std::string peerIp(ipStr);

        Logger::log("INTERMEDIARY", "JOIN_RECEIVED", "nuevo nodo: " + peerIp);

        std::thread t([this, peerIp]() {

            try {
                Socket tcpSock('s', false);
                tcpSock.Connect(peerIp.c_str(), 3031);

                std::string payload = composeHandshakePayload();
                uint8_t tipo = MSG_HANDSHAKE;
                uint32_t len = htonl((uint32_t)payload.size());

                tcpSock.Write(&tipo, 1);
                tcpSock.Write(&len, 4);

                if(!payload.empty()) {
                    tcpSock.Write(payload.c_str());
                }

                char hdrBuf[5];
                size_t rd = tcpSock.Read(hdrBuf, 5);
                if(rd >= 5 && (uint8_t)hdrBuf[0] == MSG_HANDSHAKE) {

                    uint32_t plen;
                    memcpy(&plen, &hdrBuf[1], 4);
                    plen = ntohl(plen);

                    std::string peerPayload(plen, '\0');

                    if(plen > 0) {
                        tcpSock.Read(&peerPayload[0], plen);
                    }

                    registerPeerFigures(peerPayload, peerIp);
                }

                tcpSock.Close();
                Logger::log("INTERMEDIARY", "HANDSHAKE_DONE", "con nodo=" + peerIp);

            } catch (const std::exception & e) {
                Logger::log("INTERMEDIARY", "ERROR", std::string("HANDSHAKE fallo con ") + peerIp + ": " + e.what());
            }
        });

        t.detach();
    }
}


void IntermediaryServer::receivePeerTcp() {
    
    try {

        Socket serverSock('s', false);
        serverSock.Bind(localIp.c_str(), 3031);
        serverSock.MarkPassive(10);

        Logger::log("INTERMEDIARY", "LISTEN", "TCP PEER en puerto 3031");
        std::cout << "intermediario escuchando nodos TCP en puerto 3031\n";

        while(true) {

            VSocket * peer = serverSock.AcceptConnection();
            std::thread t([this, peer]() { processHandshake(peer); });
            t.detach();
        }

    } catch (const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("receivePeerTcp: ") + e.what());
    }
}


void IntermediaryServer::processHandshake(VSocket* peer ) {

    try {
        uint8_t tipo;
        peer->Read(&tipo, 1);

        if(tipo == MSG_HANDSHAKE) {

            uint32_t netLen;
            peer->Read(&netLen, 4);
            uint32_t len = ntohl(netLen);

            std::string payload(len, '\0');

            if(len > 0) {
                peer->Read(&payload[0], len);
            }

            registerPeerFigures(payload, "peer");

            std::string myPayload = composeHandshakePayload();
            uint8_t myTipo = MSG_HANDSHAKE;
            uint32_t myLen = htonl((uint32_t)myPayload.size());

            peer->Write(&myTipo, 1);
            peer->Write(&myLen, 4);

            if(!myPayload.empty()) {
                peer->Write(myPayload.c_str());
            }

            Logger::log("INTERMEDIARY", "HANDSHAKE_RECV", "handshake completado -> figuras recibidas: " + payload);

        } else if (tipo == MSG_IR_REQUEST) {

            uint8_t segment, nameLen;
            peer->Read(&segment, 1);
            peer->Read(&nameLen, 1);

            std::string figureName(nameLen, '\0');

            if(nameLen > 0) {
                peer->Read(&figureName[0], nameLen);
            }

            Logger::log("INTERMEDIARY", "IR_REQUEST", "figura=" + figureName + " segment=" + std::to_string(segment));

            std::string protoData;
            std::string targetIp;
            int targetPort = 0;
            bool found = false;

            {
                std::lock_guard<std::mutex> lock(routingMutex);
                auto it = routingTable.find(figureName);
                if(it != routingTable.end()) {
                    targetIp   = it->second.address;
                    targetPort = it->second.portNumber;
                    found      = true;
                }
            }

            if(found) {
                protoData = fetchFigureData(targetIp, targetPort, figureName, segment);
            }

            if(!protoData.empty()) {

                uint8_t rTipo = MSG_IR_RESPONSE;
                uint8_t rSeg = segment;
                uint8_t rNameLen = (uint8_t)figureName.size();
                uint32_t rCLen = htonl((uint32_t)protoData.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rSeg, 1);
                peer->Write(&rNameLen, 1);
                peer->Write(figureName.c_str());
                peer->Write(&rCLen, 4);
                peer->Write(protoData.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE", "figura=" + figureName + " enviada");

            } else {

                uint8_t rTipo = MSG_NOT_FOUND;
                uint32_t rCLen = htonl((uint32_t)figureName.size());

                peer->Write(&rTipo, 1);
                peer->Write(&rCLen, 4);
                peer->Write(figureName.c_str());

                Logger::log("INTERMEDIARY", "IR_RESPONSE", "MSG_NOT_FOUND figura=" + figureName);
            }
        }

    } catch (const std::exception & e) {

        Logger::log("INTERMEDIARY", "ERROR", std::string("processHandshake: ") + e.what());
    }

    try { 
        peer->Close(); 

    } catch (...) {

    }

    delete peer;
}


std::string IntermediaryServer::composeHandshakePayload() const {

    std::string payload;
    bool first = true;

    for(const auto & entry : routingTable) {

        if(!first) {
            payload += ",";
        }

        payload += entry.first;
        first = false;
    }

    return payload;
}


void IntermediaryServer::registerPeerFigures(const std::string & payload, const std::string & peerIp) {

    NodeInfo info;
    info.ipAddress = peerIp;

    std::istringstream ss(payload);
    std::string token;

    while(std::getline(ss, token, ',')) {

        if(!token.empty()) {
            info.knownFigures.push_back(token);
        }
    }

    {
        std::lock_guard<std::mutex> lock(nodesMutex);

        for(auto & node : knownNodes) {

            if(node.ipAddress == peerIp) {
                node.knownFigures = info.knownFigures;
                Logger::log("INTERMEDIARY", "NODE_UPDATED", "ip=" + peerIp + " figuras=" + std::to_string(info.knownFigures.size()));

                return;
            }
        }

        knownNodes.push_back(info);
    }

    Logger::log("INTERMEDIARY", "NODE_ADDED", "ip=" + peerIp + " figuras=" + std::to_string(info.knownFigures.size()));
}
