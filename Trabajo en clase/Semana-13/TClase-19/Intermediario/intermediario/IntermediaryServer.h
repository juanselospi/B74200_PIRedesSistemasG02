#ifndef IntermediaryServer_h
#define IntermediaryServer_h

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include "Socket.h"

static constexpr uint8_t MSG_JOIN = 0;
static constexpr uint8_t MSG_HANDSHAKE = 1;
static constexpr uint8_t MSG_IR_REQUEST = 2;
static constexpr uint8_t MSG_IR_RESPONSE = 3;
static constexpr uint8_t MSG_NOT_FOUND = 4;

struct RouteEntry {

    std::string name; // nombre de la figura
    std::string address; // IP del servidor de figuras que la tiene
    int portNumber; // puerto del servidor de figuras
};


struct NodeInfo {

    std::string ipAddress;
    std::vector<std::string> knownFigures; // figuras que ese nodo conoce
};


class IntermediaryServer {
public:
    IntermediaryServer(const std::string & localIp, int servicePort, const std::string & backendIp, int backendPort );
    void start();

private:
    std::string localIp;
    int servicePort; // puerto donde atiende clientes

    std::string backendIp; // IP del servidor de figuras
    int backendPort; // puerto del servidor de figuras

    // tabla de rutas local figura -> ruta
    std::map<std::string, RouteEntry> routingTable;
    std::mutex routingMutex;

    // tabla de otros intermediarios conocidos
    std::vector<NodeInfo> knownNodes;
    std::mutex nodesMutex;

    // metodos de inicializacion
    void initRouteTable();
    std::vector<std::string> requestFigureDirectory(const std::string & ip, int port);

    // cxomunicacion con servidor de figuras
    std::string fetchFigureData(const std::string & ip, int port, const std::string & figureName, uint8_t segment);

    std::string dispatchProtoMessage(const std::string & ip, int port, const std::string & msg);

    // atencion de clientes HTTP-NachOS
    void acceptClients();
    void serveClient(VSocket* client);

    std::string routeHttpRequest(const std::string & request);
    std::string receiveRequest(VSocket* client);

    // handlers de HTTP
    std::string serveIndex( bool nachos );
    std::string servePieceList( const std::string & path, bool nachos );
    std::string composeHttpResponse( const std::string & body, const std::string & status = "200 OK", const std::string & ctype  = "text/html; charset=UTF-8" );

    // protocolo entre-intermediarios
    void receiveJoinUdp();// UDP 3030: recibe JOIN de otros forks
    void receivePeerTcp(); // TCP 3031: recibe el HANDSHAKE

    void processHandshake(VSocket* peer);
    std::string composeHandshakePayload() const;

    void registerPeerFigures(const std::string & payload, const std::string & peerIp);

    // helpers HTTP
    std::string extractPath(const std::string & request);
    std::string extractParam(const std::string & path, const std::string & key);
    bool detectNachos(const std::string & request) const;

    // helpers de conversion de piezas
    std::string convertToPlainText(const std::string & protoData);
    std::string convertToHtmlTable(const std::string & protoData, const std::string & figure, const std::string & segment);

    // los log de la tabla de rutas
    void displayRouteTable() const;
};

#endif
