/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2025-i
  *  Grupos: 1 y 3
  *
  ****** SSLSocket example 
  *
  * (Fedora version)
  *
 **/

#include <cstdlib>
#include <cstring>   // strlen
#include <cstdio>

#include "SSLSocket.h"

/**
 *
 **/
int main(int cuantos, char * argumentos[] ) {

   SSLSocket * client;
   char userName[16] = { 0 };
   char password[16] = { 0 };

   const char * requestMessage = "\n<Body>\n\
\t<UserName>%s</UserName>\n\
\t<Password>%s</Password>\n\
</Body>\n";

   char buf[1024];
   char clientRequest[1024] = { 0 };
   int bytes;
   char *hostname, *portnum;

   if (cuantos != 3) {
      printf("usage: %s <hostname> <portnum>\n", argumentos[0]);
      exit(0);
   }

   hostname = argumentos[1];
   portnum  = argumentos[2];

   client = new SSLSocket();
   
   client->Init(true);
   client->Connect(hostname, atoi(portnum));
   client->ConnectSSL();

   printf("Enter the User Name : ");
   scanf("%s", userName);

   printf("\nEnter the Password : ");
   scanf("%s", password);

   sprintf(clientRequest, requestMessage, userName, password);

   printf("\n\nConnected with %s encryption\n", client->GetCipher());

   client->ShowCerts();

   client->Write(clientRequest);

   bytes = client->Read(buf, sizeof(buf));
   buf[bytes] = 0;

   printf("Received: \"%s\"\n", buf);

   return 0;
}

/*

OUTPUT ESPERADO:

 I  ~/De/V/PI Redes y Sistemas/Tr/Semana-06/TClase-09  main !2 ?23  ./SSLClient.out 127.0.0.1 4321                ✘ ABRT  14:54:54 
Enter the User Name : piro

Enter the Password : ci0123


Connected with TLS_AES_256_GCM_SHA384 encryption
Certificates:
Subject: /C=CR/ST=localhost/L=SanJose/O=UCR/OU=ECCI/CN=localhost
Issuer: /C=CR/ST=localhost/L=SanJose/O=UCR/OU=ECCI/CN=localhost
Received: "
<Body>
	<Server>os.ecci.ucr.ac.cr</Server>
	<dir>ci0123</dir>
	<Name>Proyecto Integrador Redes y sistemas Operativos</Name>
	<NickName>PIRO</NickName>
	<Description>Consolidar e integrar los conocimientos de redes y sistemas operativos</Description>
	<Author>profesores PIRO</Author>
</Body>
"

*/