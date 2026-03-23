/**
 *
 *   UCR-ECCI
 *
 *   IPv4 TCP client normal or SSL according to parameters
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

   char * host = (char *) "142.250.190.78"; // google

   const char * request = 
   "GET / HTTP/1.1\r\n"
   "Host: google.com\r\n"
   "Connection: close\r\n\r\n";

   // char * os = (char *) "163.178.104.62";
   // char * whale = (char *) "GET /aArt/index.php?disk=Disk-01&fig=whale-1.txt\r\nHTTP/v1.1\r\nhost: redes.ecci\r\n\r\n";
//   char * request = (char *) "GET /ci0123 HTTP/1.1\r\nhost:redes.ecci\r\n\r\n";

   if (argc > 1 ) {
      port = 443;
      client = new SSLSocket();	// Create a new stream socket for IPv4
   } else {
      client = new Socket( 's' );
      port = 80;
   }

   // memset( a, 0 , MAXBUF );
   // client->Connect( os, port );
   // client->Write(  (char * ) whale, strlen( whale ) );
   // st = client->Read( a, MAXBUF );
   // printf( "Bytes read %d\n%s\n", st, a);


   memset( a, 0 , MAXBUF );
   client->Connect( host, port );
   client->Write( request, strlen( request ) );
   while ( (st = client->Read( a, MAXBUF )) > 0 ) {
      printf("%s", a);
      memset( a, 0, MAXBUF );
   }

   delete client;

   return 0;

}

