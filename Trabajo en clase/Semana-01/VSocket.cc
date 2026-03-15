/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  *******   VSocket base class implementation
  *
  * (Fedora version)
  *
 **/

#include <sys/socket.h>
#include <arpa/inet.h>		// ntohs, htons
#include <stdexcept>            // runtime_error
#include <cstring>		// memset
#include <netdb.h>		// getaddrinfo, freeaddrinfo
#include <unistd.h>		// close
#include <net/if.h>
/*
#include <cstddef>
#include <cstdio>

//#include <sys/types.h>
*/
#include "VSocket.h"


/**
  *  Class creator (constructor)
  *     use Unix socket system call
  *
  *  @param     char t: socket type to define
  *     's' for stream
  *     'd' for datagram
  *  @param     bool ipv6: if we need a IPv6 socket
  *
 **/
void VSocket::Init( char t, bool IPv6 ){

   // int st = -1;

   this->sockId = -1;
  
   this->IPv6 = IPv6;

   this->port = 0;

   this->type = t;

   int family; // dominio
   int sockType;
   int socketSO;
   int protocol = 0;

   // esto define la familia, si estoy usando IP version 6 o version 4
   if(this->IPv6 == false) {
      family = AF_INET;
   } else if(this->IPv6 == true) {
      family = AF_INET6;
   }

   // esto define el tipo de socket que estoy usando
   if(this->type == 's') {
      sockType = SOCK_STREAM; // TCP
   } else if(this->type == 'd') {
      sockType = SOCK_DGRAM; // UDP
   }

   // crear el socket del SO
   socketSO = ::socket(family, sockType, protocol); // familia, tipo(de socket), protocolo

   if ( -1 == socketSO ) {
      throw std::runtime_error( "VSocket::Init, (reason)" );
   }

   // si todo salio bien
   this->sockId = socketSO;

}


/**
  * Class destructor
  *
 **/
VSocket::~VSocket() {

   this->Close();

}


/**
  * Close method
  *    use Unix close system call (once opened a socket is managed like a file in Unix)
  *
 **/
void VSocket::Close(){
   int st = -1;

   st = close(this->sockId);

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Close(): close failed" );
   }
}


/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dot notation, example "10.84.166.62"
  * @param      int port: process address, example 80
  *
 **/
int VSocket::TryToConnect( const char * hostip, int port ) {

   int st = -1;


   if(this->IPv6 == false) {

      // IPv4
      sockaddr_in host4;
      std::memset(&host4, 0, sizeof(host4));
      host4.sin_family = AF_INET;

      st = ::inet_pton(AF_INET, hostip, &host4.sin_addr); // convierto la IP a binario

      if(st == 0) {

         throw std::runtime_error("VSocket::TryToConnect(IPv4): inet_pton: direccion invalida"); // la cadena que me dieron no es una direccion valida
      } else if(st == -1) {

         throw std::runtime_error("VSocket::TryToConnect(IPv4): inet_pton: error del sistema"); // error del SO
      }


      host4.sin_port = htons(port); // defino el puerto / htons significa host to network short


      st = ::connect(this->sockId, reinterpret_cast<sockaddr*>(&host4), sizeof(host4)); // esto intenta abrir la conexion TCP

      if ( -1 == st ) {
         throw std::runtime_error( "VSocket::TryToConnect(IPv4): connect: failed to connect" ); // fallo en la conexion
      }

   } else if(this->IPv6 == true) {

      // IPv6
      sockaddr_in6 host6; // igual que en IPv4 esta es mi estructura de datos, pero para IPv6
      std::memset(&host6, 0, sizeof(host6)); // limpia memoria interna poniendo todos los bytes en 0
      host6.sin6_family = AF_INET6; // esto define la familia de red, en este caso IPv6


      // separo la direccion IP de la interfaz
      char addressCopy[128];
      std::strncpy(addressCopy, hostip, sizeof(addressCopy));
      addressCopy[sizeof(addressCopy)-1] = '\0';

      char * interface = std::strchr(addressCopy, '%'); // busca le caracter '%'

      // si el caracter no aparece
      if(interface != nullptr) {

         *interface = '\0'; // cortar la direccion IPv6
         interface++; // mover el puntero al nombre de la interfaz

         if(*interface == '\0') {
            throw std::runtime_error("VSocket::TryToConnect(IPv6): interface quedo vacia");
         }

         // implementar interface
         host6.sin6_scope_id = if_nametoindex(interface);

         // verifico que la interfaz exista ne primer lugar
         if(host6.sin6_scope_id == 0) {
            throw std::runtime_error("VSocket::TryToConnect(IPv6): interface no existe");
         }
      }

      st = ::inet_pton(AF_INET6, addressCopy, &host6.sin6_addr); // convierto la parte IP que separe adressCopy y NO hostip

      if(st == 0) {

         throw std::runtime_error("VSocket::TryToConnect(IPv6): inet_pton: direccion invalida"); // la cadena que me dieron no es una direccion valida
      } else if(st == -1) {

         throw std::runtime_error("VSocket::TryToConnect(IPv6): inet_pton: error del sistema"); // error del SO
      }

      host6.sin6_port = htons(port); // puerto definido para IPv6

      st = ::connect(sockId, reinterpret_cast<sockaddr*>(&host6), sizeof(host6));

      if ( -1 == st ) {
         throw std::runtime_error( "VSocket::TryToConnect(IPv6): connect: failed to connect" ); // fallo en la conexion
      }

   }

   return 0;
}


/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dns notation, example "os.ecci.ucr.ac.cr"
  * @param      char * service: process address, example "http"
  *
 **/
int VSocket::TryToConnect( const char *host, const char *service ) {
   int st = -1;

   struct servent * serviceEntry;

   serviceEntry = getservbyname(service, "tcp"); // obtengo la info del servicio http -> 80

   if(serviceEntry == nullptr) {

      throw std::runtime_error( "VSocket::TryToConnect: servicio no encontrado" );
   }

   int port = ntohs(serviceEntry->s_port); // puerto de network a host

   st = this->TryToConnect(host, port); // llama a la version de arriba de TryToConnect

   return st;
}

