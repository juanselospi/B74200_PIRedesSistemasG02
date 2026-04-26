#include "SSLSocket.h"
#include <iostream>
#include <stdexcept>



SSLSocket::SSLSocket(char type, bool IPv6) : Socket(type, IPv6) {
   this->context = nullptr;
   this->ssl = nullptr;
}

SSLSocket::SSLSocket(int id) : Socket(id) {
   this->context = nullptr;
   this->ssl = nullptr;
}

SSLSocket::SSLSocket(const char *cert, const char *key, bool IPv6) : Socket('s', IPv6) {

   this->context = nullptr;
   this->ssl = nullptr;

   this->InitContext(true);

   this->LoadCertificates(cert, key);
}


SSLSocket::~SSLSocket() {

   if(this->ssl) {

      SSL_free(this->ssl);
   }
}


void SSLSocket::ConnectSSL() {

   SSL_set_fd(this->ssl, this->sockId);

   int st = SSL_connect(this->ssl);

   if(st <= 0) {

      ERR_print_errors_fp(stderr);
      abort();
   }

}


void SSLSocket::InitContext(bool server) {

   const SSL_METHOD *method;

   if(server) {

      SSL_library_init();
      OpenSSL_add_all_algorithms();
      SSL_load_error_strings();

      method = TLS_server_method();

   } else {

      method = TLS_client_method();
   }

   this->context = SSL_CTX_new(method);

   if(this->context == nullptr) {

      ERR_print_errors_fp(stderr);
      abort();
   }
}

void SSLSocket::Init(bool server) {

   this->InitContext(server);

   this->ssl = SSL_new(this->context);

   if(this->ssl == nullptr) {

      ERR_print_errors_fp(stderr);
      abort();
   }

}


void SSLSocket::Attach(Socket *sock) {

   this->sockId = sock->GetSockId();

   SSL_set_fd(this->ssl, this->sockId);
}


void SSLSocket::Copy(SSLSocket *original) {

   this->context = original->context;

   this->ssl = SSL_new(this->context);

   if(!this->ssl) {

      ERR_print_errors_fp(stderr);
      abort();
   }

   SSL_set_fd(this->ssl, this->sockId);
}

void SSLSocket::Accept() {

   int st = SSL_accept(this->ssl);

   if(st <= 0) {

      ERR_print_errors_fp(stderr);
      abort();
   }
}

VSocket * SSLSocket::AcceptConnection() {

   int id = this->WaitForConnection();

   SSLSocket *client = new SSLSocket(id);

   return client;
}

size_t SSLSocket::Read(void *buffer, size_t size) {

   int st = SSL_read(this->ssl, buffer, size);

   if(st <= 0) {

      ERR_print_errors_fp(stderr);
      throw std::runtime_error("SSL Read error");
   }

   return st;
}

size_t SSLSocket::Write(const void *buffer, size_t size) {

   int st = SSL_write(this->ssl, buffer, size);

   if(st <= 0) {

      ERR_print_errors_fp(stderr);
      throw std::runtime_error("SSL Write error");
   }

   return st;
}

size_t SSLSocket::Write(const char *text) {

   size_t len = strlen(text);

   return this->Write((const void *)text, len);
}

const char * SSLSocket::GetCipher() {

   return SSL_get_cipher(this->ssl);
}

void SSLSocket::LoadCertificates(const char *certFile, const char *keyFile) {

   if(SSL_CTX_use_certificate_file(this->context, certFile, SSL_FILETYPE_PEM) <= 0) {

      ERR_print_errors_fp(stderr);
      abort();
   }

   if(SSL_CTX_use_PrivateKey_file(this->context, keyFile, SSL_FILETYPE_PEM) <= 0) {

      ERR_print_errors_fp(stderr);
      abort();
   }

   if(!SSL_CTX_check_private_key(this->context)) {

      fprintf(stderr, "Private key does not match the certificate public key\n");
      abort();
   }
}

void SSLSocket::ShowCerts() {

   X509 *cert;
   char *line;

   cert = SSL_get_peer_certificate(this->ssl);

   if(cert != nullptr) {

      printf("Certificates:\n");

      line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
      printf("Subject: %s\n", line);
      free(line);

      line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
      printf("Issuer: %s\n", line);
      free(line);

      X509_free(cert);

   } else {
    
      printf("No certificates.\n");
   }
}

