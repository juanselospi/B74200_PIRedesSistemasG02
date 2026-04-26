/**
 * SSL Fork Server (IPv6)
 */

#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "SSLSocket.h"

#define PORT 4321

void Service(SSLSocket *client) {

   char buf[1024] = {0};
   int bytes;

   const char *validMessage = "\n<Body>\n\
\t<UserName>piro</UserName>\n\
\t<Password>ci0123</Password>\n\
</Body>\n";

   const char *ServerResponse = "\n<Body>\n\
\t<Server>os.ecci.ucr.ac.cr</Server>\n\
\t<dir>ci0123</dir>\n\
\t<Name>Proyecto Integrador Redes y sistemas Operativos</Name>\n\
\t<NickName>PIRO</NickName>\n\
\t<Description>Consolidar e integrar los conocimientos</Description>\n\
\t<Author>profesores PIRO</Author>\n\
</Body>\n";

   client->Accept();
   client->ShowCerts();

   bytes = client->Read(buf, sizeof(buf));
   buf[bytes] = '\0';

   printf("Client msg: \"%s\"\n", buf);

   if (!strcmp(validMessage, buf)) {
      client->Write(ServerResponse, strlen(ServerResponse));
   } else {
      client->Write("Invalid Message", strlen("Invalid Message"));
   }

   client->Close();
}

int main(int argc, char **argv) {

   int port = PORT;

   if (argc > 1) {
      port = atoi(argv[1]);
   }

   SSLSocket *server = new SSLSocket("ci0123.pem", "key0123.pem", true); // IPv6

   server->Bind(port);
   server->MarkPassive(10);

   while (true) {

      SSLSocket *client = (SSLSocket *)server->AcceptConnection();

      pid_t pid = fork();

      if (pid == 0) {  // proceso hijo

         client->Copy(server);

         Service(client);

         exit(0);
      }
   }
}
