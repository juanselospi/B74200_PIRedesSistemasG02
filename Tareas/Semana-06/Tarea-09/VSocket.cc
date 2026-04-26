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
void VSocket::Init( char t, bool IPv6 ) {

   this->type = t;
   this->IPv6 = IPv6;

   int domain = this->IPv6 ? AF_INET6 : AF_INET;

   int sockType;

   if( t == 's' ) {

      sockType = SOCK_STREAM;

   } else if( t == 'd' ) {

      sockType = SOCK_DGRAM;

   }

   this->sockId = socket(domain, sockType, 0);

   if(this->sockId == -1) {

      throw std::runtime_error( "VSocket::Init socket failed" );
   }

   if(this->IPv6) {

      int opt = 0;
      setsockopt(this->sockId, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
   }

}


/**
  *  Class creator (constructor)
  *     use Unix socket system call
  *
  *  @param     int id: socket identifier
  *
 **/
void VSocket::Init( int id ){

   int st = -1;

   this->sockId = id;

   if( id < 0 ) {
        throw std::runtime_error( "VSocket::Init invalid socket id" );
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

    int st = -1;

    if( this->sockId >= 0 ) {

        st = close(this->sockId);

        if( -1 == st ) {
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
int VSocket::TryToConnect(const char *host, int port) {

   struct addrinfo hints, *res, *p;
   int st;

   memset(&hints, 0, sizeof(hints));

   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = (this->type == 's') ? SOCK_STREAM : SOCK_DGRAM;

   char portStr[10];
   sprintf(portStr, "%d", port);

   st = getaddrinfo(host, portStr, &hints, &res);

   if( st != 0 ) {
      throw std::runtime_error( "VSocket::TryToConnect fail " );
   }

   for(p = res; p != NULL; p = p->ai_next) {

      int newSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

      if(newSock == -1) {
         continue;
      }

      if(connect(newSock, p->ai_addr, p->ai_addrlen) == 0) {

         this->sockId = newSock;
         freeaddrinfo(res);
         return 0;
      }

      close(newSock);
   }

   freeaddrinfo(res);
   throw std::runtime_error( "VSocket::TryToConnect fail" );
}




/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dns notation, example "os.ecci.ucr.ac.cr"
  * @param      char * service: process address, example "http"
  *
 **/
// int VSocket::TryToConnect( const char *host, const char *service ) {
//    int st = -1;

//    throw std::runtime_error( "VSocket::TryToConnect" );

//    return st;

// }
int VSocket::TryToConnect(const char *host, const char *service) {

   int st = -1;

   struct addrinfo hints, *res, *p;

   memset(&hints, 0, sizeof(hints));

   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = (this->type == 's') ? SOCK_STREAM : SOCK_DGRAM;

   st = getaddrinfo(host, service, &hints, &res);

   if( st != 0 ) {
      throw std::runtime_error("VSocket::TryToConnect fail");
   }

   for(p = res; p != NULL; p = p->ai_next) {

      int newSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if(newSock == -1) {
         continue;
      }

      if(connect(newSock, p->ai_addr, p->ai_addrlen) == 0) {

         this->sockId = newSock;
         freeaddrinfo(res);
         return 0;
      }

      close(newSock);
   }

   freeaddrinfo(res);
   throw std::runtime_error("VSocket::TryToConnect fail");
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

   struct addrinfo hints, *res, *p;

   memset(&hints, 0, sizeof(hints));

   hints.ai_family = this->IPv6 ? AF_INET6 : AF_INET;
   hints.ai_socktype = (this->type == 's') ? SOCK_STREAM : SOCK_DGRAM;

   hints.ai_flags = AI_PASSIVE;

   char portStr[10];

   sprintf(portStr, "%d", port);

   st = getaddrinfo(NULL, portStr, &hints, &res);

   if( st != 0 ) {

      throw std::runtime_error("Bind getaddrinfo error");
   }

   for(p = res; p != NULL; p = p->ai_next) {

      if(bind(this->sockId, p->ai_addr, p->ai_addrlen) == 0) {

         freeaddrinfo(res);
         return 0;

      }
   }

   freeaddrinfo(res);
   throw std::runtime_error("Bind error");
}




/**
  * MarkPassive method
  *    use "listen" Unix system call (man listen) (server mode)
  *
  * @param      int backlog: defines the maximum length to which the queue of pending connections for this socket may grow
  *
  *  Establish socket queue length
  *
 **/
int VSocket::MarkPassive( int backlog ) {

   int st = -1;  

   st = listen(this->sockId, backlog);

   if( -1 == st ) {
      throw std::runtime_error( "VSocket::MarkPassive fail" );
   }

   return st;

}


/**
  * WaitForConnection method
  *    use "accept" Unix system call (man 3 accept) (server mode)
  *
  *
  *  Waits for a peer connections, return a sockfd of the connecting peer
  *
 **/
int VSocket::WaitForConnection( void ) {

   int st = -1;

   struct sockaddr_in client_addr;
   socklen_t len = sizeof(client_addr);

   int newSock = accept(this->sockId, (struct sockaddr *)&client_addr, &len);

   if( -1 == newSock ) {
      throw std::runtime_error( "VSocket::WaitForConnection fail" );
   }

   return newSock;

}


/**
  * Shutdown method
  *    use "shutdown" Unix system call (man 3 shutdown) (server mode)
  *
  *
  *  cause all or part of a full-duplex connection on the socket associated with the file descriptor socket to be shut down
  *
 **/
int VSocket::Shutdown( int mode ) {

   int st = -1;

   st = shutdown(this->sockId, mode);

   if( -1 == st ) {
      throw std::runtime_error( "VSocket::Shutdown fail" );
   }

   return st;

}


// UDP methods 2025

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

   return st;

}
