# Linux Makefile

CC  := gcc
CXX := g++
CFLAGS := -g -c
CXXFLAGS := -g -c -std=c++11
LDFLAGS := -lssl -lcrypto -pthread

DEPS := VSocket.h Socket.h SSLSocket.h

TARGETS := SSLServer.out SSLClient.out SSLThreadServer.out SSLForkServer.out

all: $(TARGETS)
$(info $$TARGETS = $(TARGETS))

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $<

SSLServer.out: SSLServer.o SSLSocket.o Socket.o VSocket.o
	$(CXX) $^ $(LDFLAGS) -o $@

SSLClient.out: SSLClient.o SSLSocket.o Socket.o VSocket.o
	$(CXX) $^ $(LDFLAGS) -o $@

SSLThreadServer.out: SSLThreadServer.o SSLSocket.o Socket.o VSocket.o
	$(CXX) $^ $(LDFLAGS) -o $@

SSLForkServer.out: SSLForkServer.o SSLSocket.o Socket.o VSocket.o
	$(CXX) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o *.out