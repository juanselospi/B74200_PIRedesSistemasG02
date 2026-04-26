#ifndef SSLSocket_h
#define SSLSocket_h

#include "Socket.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

class SSLSocket : public Socket {

   private:
      SSL_CTX *context;
      SSL *ssl;

   public:
      SSLSocket(char type = 's', bool IPv6 = false);
      SSLSocket(int id);
      SSLSocket(const char *cert, const char *key, bool IPv6);
      ~SSLSocket();

      void ConnectSSL();

      void InitContext(bool server);
      void Init(bool server);
      void LoadCertificates(const char *, const char *);

      void Copy(SSLSocket *original);
      void Attach(Socket *sock);

      void Accept();
      VSocket * AcceptConnection() override;

      size_t Read(void *, size_t);
      size_t Write(const void *, size_t);
      size_t Write(const char *);

      void ShowCerts();
      const char *GetCipher();
};

#endif
