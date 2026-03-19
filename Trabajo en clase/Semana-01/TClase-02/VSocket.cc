/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  ****** VSocket base class implementation
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

   this->sockId = -1;

   this->type = t;

   this->IPv6 = IPv6;

   int family; // dominio
   int SocketType;

   // selecciono dominio entre IPv4 y IPv6
   if(this->IPv6 == false) {
      family = AF_INET;
   } else {
      family = AF_INET6;
   }

   // selecion del tipo de socket
   if(this->type == 's') {
      SocketType = SOCK_STREAM; // TCP
   } else {
      SocketType = SOCK_DGRAM; // UDP
   }

   // CREO EL SOCKET
   this->sockId = socket(family, SocketType, 0);

   if ( -1 == this->sockId ) {
      throw std::runtime_error( "VSocket::Init, (reason)" );
   }
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
   
   if(this->sockId != -1) {
   
      int st = close(this->sockId); // cierro el socket

      if ( -1 == st ) {
         throw std::runtime_error( "VSocket::Close()" );
      }
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

   // SOLO PARA IPv4 PORQUE NO USO EL OTRO AQUI
   struct sockaddr_in host4;

   host4.sin_family = AF_INET;
   host4.sin_port = htons(port);

   inet_pton(AF_INET, hostip, &host4.sin_addr);

   memset(host4.sin_zero, 0, sizeof(host4.sin_zero));

   st = connect(this->sockId, (struct sockaddr*) &host4, sizeof(host4));

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::TryToConnect: connection failed" );
   }

   return st;
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

   throw std::runtime_error( "VSocket::TryToConnect" );

   return st;

}


/**
  * Bind method
  *    use "bind" Unix system call (man 3 bind) (server mode)
  *
  * @param      int port: bind a unamed socket to a port defined in sockaddr structure
  *
  *  Links the calling process to a service at port
  *
 **/
int VSocket::Bind( int port ) {
   
   int st = -1;

   struct sockaddr_in host4;

   this->port = port;

   host4.sin_family = AF_INET;

   host4.sin_addr.s_addr = htonl(INADDR_ANY);

   host4.sin_port = htons(port);

   memset(host4.sin_zero, '\0', sizeof(host4.sin_zero));

   st = bind(this->sockId, (struct sockaddr*) &host4, sizeof(host4));

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Bind()" );
   }

   return st;
}


/**
  *  sendTo method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to send data
  *
  *  Send data to another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::sendTo( const void * buffer, size_t size, void * addr ) {
   int st = -1;

   st = sendto(this->sockId, buffer, size, 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in));

   if( -1 == st ) {
      throw std::runtime_error( "VSocket::sendTo()" );
   }

   return st;
}


/**
  *  recvFrom method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to receive from data
  *
  *  @return	size_t bytes received
  *
  *  Receive data from another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::recvFrom( void * buffer, size_t size, void * addr ) {
   int st = -1;

   socklen_t addr_len = sizeof(struct sockaddr_in);

   st = recvfrom(this->sockId, buffer, size, 0, (struct sockaddr*) addr, &addr_len);

   if( -1 == st ) {
      throw std::runtime_error( "VSocket::recvFrom()" );
   }

   return st;

}

