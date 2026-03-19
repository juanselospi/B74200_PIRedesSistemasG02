/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  * (Fedora version)
  *
  *   Client side implementation of IPv6 UDP client-server model 
  *
 **/

#include <stdio.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>

#include "Socket.h"
#include <net/if.h>

#define PORT    1234 
#define MAXLINE 1024 

int main() {
   VSocket * client;
   int sockfd; 
   int n, len; 
   char buffer[MAXLINE]; 
   char *hello = (char *) "Hello from client 2026"; 
   struct sockaddr_in6 other;

   client = new Socket( 'd', true );

   memset( &other, 0, sizeof( other ) ); 
   
   other.sin6_family = AF_INET6; 
   other.sin6_port = htons(PORT); 

   // DESCOMENTAR ESTO PARA CORRER EN EL LAB
   // inet_pton(AF_INET6, "fe80::8f5a:e2e1:7256:ffe3", &other.sin6_addr);
   // other.sin6_scope_id = if_nametoindex("enp0s31f6");




   // PARA CORRER LOCALMENTE USO EL ::1
   inet_pton(AF_INET6, "::1", &other.sin6_addr);
   

   n = client->sendTo( (void *) hello, strlen( hello ), (void *) & other ); 
   printf("Client: Hello message sent.\n"); 
   
   n = client->recvFrom( (void *) buffer, MAXLINE, (void *) & other );
   buffer[n] = '\0'; 
   printf("Client message received: %s\n", buffer); 

   client->Close(); 

   return 0;
 
} 


// PRUEBA DE QUE CORRIO EL PROGRAMA

/* TERMINAL 1

 I  ~/Desktop/VSCode/PI Redes y Sistemas/Tareas/Semana-01/Tarea-02  main !7  make                                      ✔  22:04:31 
$TARGETS = ipv6-udp-client.out ipv6-udp-server.out
g++ -g -c ipv6-udp-client.cc
g++ -g -c VSocket.cc
g++ -g -c Socket.cc
g++ -g ipv6-udp-client.o VSocket.o Socket.o -o ipv6-udp-client.out
 I  ~/Desktop/VSCode/PI Redes y Sistemas/Tareas/Semana-01/Tarea-02  main !9 ?4  make ipv6-udp-server.out             ✘ 2  22:52:02 
$TARGETS = ipv6-udp-client.out ipv6-udp-server.out
g++ -g -c ipv6-udp-server.cc
g++ -g ipv6-udp-server.o VSocket.o Socket.o -o ipv6-udp-server.out
 I  ~/Desktop/VSCode/PI Redes y Sistemas/Tareas/Semana-01/Tarea-02  main !9 ?6  ./ipv6-udp-server.out                  ✔  22:53:26 
Server: message received: Hello from client 2026
Server: Hello message sent.


TERMINAL 2

 I  ~/Desktop/VSCode/PI Redes y Sistemas/Tareas/Semana-01/Tarea-02  main !9 ?6  ./ipv6-udp-client.out                  ✔  22:53:38 
Client: Hello message sent.
Client message received: Hello from B74200 server 2026


*/

