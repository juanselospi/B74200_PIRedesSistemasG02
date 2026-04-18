/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  *   Socket client/server example with threads
  *
  *
 **/
 
#include <iostream>
#include <thread>

#include "Socket.h"

#define PORT 2026
#define BUFSIZE 512


/**
 *   Task each new thread will run
 *      Read string from socket
 *      Write it back to client
 *
 **/
void task( VSocket * client ) {
   char a[ BUFSIZE ];

   int n = client->Read(a, BUFSIZE);
   a[n] = '\0';

   std::string request(a);
   std::string response;

   if(request == "Falcon") {

      response = "Millennium Falcon";

   } else if(request == "XWing") {

      response = "X-Wing";

   } else {

      response = "FIGURE_NOT_FOUND";
   }

   std::cout << "Server received: " << request << std::endl;

   client->Write(response.c_str());

   delete client;

}


/**
 *   Create server code
 *      Infinite for
 *         Wait for client conection
 *         Spawn a new thread to handle client request
 *
 **/
int main( int argc, char ** argv ) {
   std::thread * worker;
   VSocket * s1, * client;

   s1 = new Socket( 's', true );

   s1->Bind( PORT );		// Port to access this mirror server
   s1->MarkPassive( 5 );	// Set socket passive and backlog queue to 5 connections

   for( ; ; ) {
      client = s1->AcceptConnection();	 	// Wait for a client connection

      // worker = new std::thread( task, client );

      std::thread( task, client ).detach();
   }

}
