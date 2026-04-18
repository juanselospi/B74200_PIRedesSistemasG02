/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  *   Socket client/server example
  *
  * (Fedora version)
  *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>	// memset
#include <unistd.h>
#include <string>

#include "Socket.h"

#define PORT 2026
#define BUFSIZE 512

int main( int argc, char ** argv ) {
   VSocket * s1, * s2;
   int childpid;
   char a[ BUFSIZE ];

   s1 = new Socket( 's', true );  

   s1->Bind( PORT );       // Port to access this mirror server
   s1->MarkPassive( 5 );      // Set passive socket and backlog queue to 5 connections

   for( ; ; ) {
      s2 = s1->AcceptConnection();  // Wait for a new connection
      childpid = fork();      // Create a child to serve the request
      if ( childpid < 0 ) {
         perror( "server: fork error" );
      } else {
         if (0 == childpid) {    // child code
            s1->Close();      // Close original socket in child
            memset( a, 0, BUFSIZE );

            int n = s2->Read(a, BUFSIZE);
            a[n] = '\0';

            std::string request(a);
            std::string response;

            if(request == "Falcon") {

               response = "Millennium Falcon";

            } else if (request == "XWing") {

               response = "X-Wing";

            } else {

               response = "FIGURE_NOT_FOUND";

            }

            s2->Write(response.c_str());
            s2->Close();
            exit( 0 );
         }
      }

      s2->Close();      // Close socket s2 in parent, then go wait for a new conection

   }

}
