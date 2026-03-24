/**
 *
 *   UCR-ECCI
 *
 *   IPv6 TCP client normal or SSL according to parameters
 *
 **/

#include <stdio.h>
#include <string.h>

#include "VSocket.h"
#include "Socket.h"
#include "SSLSocket.h"

#define	MAXBUF	1024

/**
**/
int main( int argc, char * argv[] ) {
   VSocket * client;
   int st, port = 80;
   char a[ MAXBUF ];

   const char * host = "redes.ecci.ucr.ac.cr";
   const char * service;

   const char * whale =
   "GET /aArt/index.php?disk=Disk-01&fig=whale-1.txt HTTP/1.1\r\n"
   "Host: redes.ecci\r\n"
   "Connection: close\r\n\r\n";

   if (argc > 1 ) {
      service = "443";
      client = new SSLSocket();
   } else {
      service = "80";
      client = new Socket( 's' );
   }

   memset( a, 0 , MAXBUF );

   client->Connect( host, service );

   client->Write( whale, strlen( whale ) );

   while ( (st = client->Read( a, MAXBUF )) > 0 ) {
      printf("%s", a);
      memset(a, 0, MAXBUF);
   }

   delete client;

   return 0;

}

