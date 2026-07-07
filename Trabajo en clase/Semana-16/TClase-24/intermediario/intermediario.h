#ifndef intermediario_h
#define intermediario_h

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "Socket.h"

// tipos de paquete para el protocolo inter-intermediario
static constexpr uint8_t PKT_JOIN = 0;
static constexpr uint8_t PKT_HANDSHAKE = 1;
static constexpr uint8_t PKT_IR_REQUEST = 2;
static constexpr uint8_t PKT_IR_RESPONSE = 3;
static constexpr uint8_t PKT_NOT_FOUND = 4;

// puertos del protocolo inter-intermediario
static constexpr int PORT_JOIN_UDP  = 3030; // UDP -> envio/recepcion de JOIN
static constexpr int PORT_PEER_TCP  = 3031; // TCP -> HANDSHAKE / INTERMEDIARY_REQUEST

// UDP para descubrir otros intermediarios
static constexpr int PORT_REMOTE_DISC = 2027;

// UDP para que el intermediario encuentre a su servidor de figuras
static constexpr int PORT_SERVER_DISC = 8090;

// figura por mi servidor -> Pt, o por un intermediario remoto HTTP
enum class RouteKind { LOCAL_PT, REMOTE_HTTP };

// entrada en la tabla de rutas
struct FigureRoute {
    std::string figureName;
    std::string serverIp;
    int serverPort;
    RouteKind kind = RouteKind::LOCAL_PT;
};

// entrada para otros intermediarios conocidos
struct PeerInfo {
    std::string ip;
    std::vector<std::string> figures; // figuras que ese peer conozca
};

// otro intermediario remoto que conozco por descubrimiento
struct HttpPeer {
    std::string ip;
    int port; // puerto de su intermediario
    bool merged = false; // si ya copie sus figuras a mi tabla
};

class IntermediaryServer {
public:
    IntermediaryServer(const std::string & bindIp, int clientPort, const std::string & figureServerIp, int figureServerPort, const std::string & broadcastAddr = "255.255.255.255", const std::string & seedPeerIp = "", int seedPeerPort = 2026);
    
    void run();

private:
    std::string bindIp;
    int clientPort; // puerto donde atiende clientes
    std::string figureServerIp; // IP del servidor de figuras
    int figureServerPort; // puerto del servidor de figuras

    // conexion con intermediarios remotos
    std::string broadcastAddr; // a donde mando el broadcast
    std::string seedPeerIp; // intermediario remoto pasado por CLI
    int seedPeerPort; // su puerto

    // tabla de rutas local de la forma figura -> ruta
    std::map<std::string, FigureRoute> routeTable;
    std::mutex routeTableMutex;

    // tabla de otros intermediarios peers
    std::vector<PeerInfo> peers;
    std::mutex peersMutex;

    // tabla de intermediarios remotos HTTP
    std::vector<HttpPeer> httpPeers;
    std::mutex httpPeersMutex;

    // inicializacion
    void buildLocalRouteTable(); // arma la tabla con las figuras de mi servidor
    bool discoverMyServer(); // busca mi servidor por UDP
    std::vector<std::string> fetchFigureList(const std::string & ip, int port);

    // comunicacion con servidor de figuras
    std::string queryFigureServer(const std::string & ip, int port, const std::string & figureName, uint8_t half);

    std::string sendProtoMessage(const std::string & ip, int port, const std::string & msg);

    // intermediarios remotos http
    std::string queryRemoteIntermediary(const std::string & ip, int port, const std::string & figureName, uint8_t half);

    std::vector<std::string> fetchRemoteFigureList(const std::string & ip, int port);

    // pasar entre el formato del server remoto y mi formato interno -> qty|desc
    std::string httpBodyToProtoData(const std::string & httpResponse);

    std::string protoDataToHttpBody(const std::string & protoData, const std::string & figureName, uint8_t half);

    std::string extractHttpBody(const std::string & httpResponse) const;

    int extractHttpStatus(const std::string & httpResponse) const;

    // atencion de clientes HTTP -> NachOS
    void listenClients();
    void handleClient(VSocket* client);
    std::string processHttpRequest(const std::string & request);
    std::string readRequest(VSocket* client);

    // handlers HTTP
    std::string handleIndex(bool nachos);

    std::string handleList(const std::string & path, bool nachos);

    std::string buildHttpResponse(const std::string & body, const std::string & status = "200 OK", const std::string & ctype  = "text/html; charset=UTF-8");

    // responder cuando otro intermediario me pide figuras a mi
    std::string handleHttpList();
    std::string handleHttpFigure(const std::string & path);

    // descubrimiento con otros intermediarios -> UDP 2027
    void announceToPeers();
    void listenHttpPeers();
    void addHttpPeerAndMerge(const std::string & ip, int port);

    // protocolo inter-intermediario
    void listenJoinUdp(); // UDP PORT_JOIN_UDP -> recibe JOIN de otros intermediarios
    void broadcastJoinUdp(); // UDP -> anuncia periodicamente este intermediario a otros
    void listenPeerTcp(); // TCP PORT_PEER_TCP -> recibe HANDSHAKE / INTERMEDIARY_REQUEST

    void handleHandshake(VSocket * peer);
    std::string buildHandshakePayload() const;

    void parseHandshake(const std::string & payload, const std::string & peerIp);

    // helpers de red
    std::string getPeerIp(VSocket* sock) const;

    // helpers HTTP
    std::string getPathFromRequest(const std::string & request);
    std::string getQueryParam(const std::string & path, const std::string & key);

    bool isNachosRequest(const std::string & request) const;

    // helpers de conversion de piezas --
    std::string protoDataToNachos(const std::string & protoData);
    std::string protoDataToHtml(const std::string & protoData, const std::string & figure, const std::string & part);

    // log de tabla de rutas
    void printRouteTable() const;
};

#endif
