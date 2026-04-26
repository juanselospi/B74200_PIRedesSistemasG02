/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-i
  *  Grupos: 2 y 3
  *
  ****** Socket class interface
  *
  * (Fedora version)
  *
 **/

#ifndef Socket_h
#define Socket_h
#include "VSocket.h"

#include <cstddef>

class Socket : public VSocket {

   public:
      Socket( char, bool = false );
      ~Socket();
      int Connect( const char *, int );
      int Connect( const char *, const char * );
      size_t Read( void *, size_t );
      size_t Write( const void *, size_t );
      size_t Write( const char * );

      VSocket * AcceptConnection();

      Socket( int );

   protected:

};

#endif
