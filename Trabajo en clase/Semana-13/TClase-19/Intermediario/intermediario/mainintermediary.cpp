#include "IntermediaryServer.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char * argv[]) {

    std::string localIp = "0.0.0.0";
    int servicePort = 8081;

    std::string backendIp = "127.0.0.1";
    int backendPort = 8080;

    if(argc >= 2) {
        localIp = argv[1];
    }

    if(argc >= 3) {
        servicePort = std::atoi(argv[2]);
    }

    if(argc >= 4) {
        backendIp = argv[3];
    }

    if(argc >= 5) {
        backendPort = std::atoi(argv[4]);
    }

    std::cout << "Servidor Intermediario Lego\n";
    std::cout << "Clientes en: " << localIp << ":" << servicePort << "\n";
    std::cout << "Servidor de figuras: " << backendIp << ":" << backendPort << "\n";
    std::cout << "UDP JOIN en:      *:3030\n";
    std::cout << "TCP Nodos en:     " << localIp << ":3031\n";

    try {
        IntermediaryServer server(localIp, servicePort, backendIp, backendPort);
        server.start();

    } catch(const std::exception & e) {

        std::cerr << "Error fatal en el intermediario: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
