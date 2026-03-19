/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2025-i
  *  Grupos: 1 y 3
  *
  ****** Socket class interface
  *
  * (Fedora version)
  *
  *  Server-side implementation of UDP client-server model	
  *
 **/

#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "VSocket.h" 
#include "Socket.h" 

#define PORT	1234 
#define MAXLINE 1024 

int main() { 
   VSocket * server;
   int len, n; 
   int sockfd;
   struct sockaddr_in other;
   char buffer[MAXLINE]; 
   char *hello = (char *) "Hello from B74200 server"; 
	
   server = new Socket( 'd', false );
   server->Bind( PORT );

   memset( &other, 0, sizeof( other ) );

   n = server->recvFrom( (void *) buffer, MAXLINE, (void *) &other );	// Mensaje de los www servers
   buffer[n] = '\0'; 
   printf("Server: message received: %s\n", buffer);

   server->sendTo( (const void *) hello, strlen( hello ), (void *) &other );
   printf("Server: Hello message sent.\n"); 

   server->Close();
   
   return 0;

} 


// OUTPUTS OBTENIDOS EN PRUEBAS LOCALES

// TERMINAL 1

// I  ~/De/VSCode/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main ?2  make                                ✔  21:51:09 
// $TARGETS = ipv4-udp-client.out ipv4-udp-server.out
// g++ -g -c ipv4-udp-client.cc
// g++ -g -c VSocket.cc
// g++ -g -c Socket.cc
// g++ -g ipv4-udp-client.o VSocket.o Socket.o -o ipv4-udp-client.out
//  I  ~/De/VSCode/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main ?2  make ipv4-udp-server.out            ✔  21:52:07 
// $TARGETS = ipv4-udp-client.out ipv4-udp-server.out
// g++ -g -c ipv4-udp-server.cc
// g++ -g ipv4-udp-server.o VSocket.o Socket.o -o ipv4-udp-server.out
//  I  ~/De/VSCode/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main ?2  ./ipv4-udp-server.out               ✔  21:52:13 
// Server: message received: Hello from B74200 client
// Server: Hello message sent.


// TERMINAL 2

// I  ~/De/VSCode/PI Redes y Sistemas/Trabajo en clase/Semana-01/TClase-02  main ?2  ./ipv4-udp-client.out               ✔  21:51:00 
// Client: Hello message sent.
// Client message received: Hello from B74200 server

