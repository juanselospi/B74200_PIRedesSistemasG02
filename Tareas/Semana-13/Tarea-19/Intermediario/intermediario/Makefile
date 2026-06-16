# ================================================
# Makefile - Servidor Intermediario Lego
# Universidad de Costa Rica - ECCI CI0123 2026-i
# ================================================

CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread

# Fuentes comunes (copiados del servidor de figuras)
COMMON_SRC = VSocket.cc Socket.cc

# Fuentes del intermediario
INTER_SRC  = IntermediaryServer.cpp mainintermediary.cpp

# Todos los objetos
ALL_SRC    = $(COMMON_SRC) $(INTER_SRC)

# Target principal
.PHONY: all clean run help

all: intermediario

intermediario: $(ALL_SRC) IntermediaryServer.h Logger.h Socket.h VSocket.h
	$(CXX) $(CXXFLAGS) -o intermediario $(ALL_SRC)
	@echo ""
	@echo "Compilacion exitosa. Ejecutar con:"
	@echo "  ./intermediario [bindIp] [clientPort] [figServerIp] [figServerPort]"
	@echo ""
	@echo "Ejemplo (por defecto):"
	@echo "  ./intermediario 0.0.0.0 8081 127.0.0.1 8080"
	@echo ""

run: intermediario
	./intermediario

clean:
	rm -f intermediario *.o

help:
	@echo "Targets disponibles:"
	@echo "  all / intermediario  - Compilar el servidor intermediario"
	@echo "  run                  - Compilar y ejecutar con valores por defecto"
	@echo "  clean                - Eliminar binarios y objetos"
	@echo ""
	@echo "Parametros de ejecucion:"
	@echo "  ./intermediario [bindIp] [clientPort] [figServerIp] [figServerPort]"
	@echo ""
	@echo "  bindIp        - IP de escucha para clientes (default: 0.0.0.0)"
	@echo "  clientPort    - Puerto HTTP/NachOS (default: 8081)"
	@echo "  figServerIp   - IP del servidor de figuras (default: 127.0.0.1)"
	@echo "  figServerPort - Puerto del servidor de figuras (default: 8080)"
