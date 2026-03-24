# Linux Makefile
# Socket class with SSL
#

CC  := gcc
CXX := g++
CFLAGS := -g -c
CXXFLAGS := -g -c 
LDFLAGS := -lssl -lcrypto
DEPS := Socket.h
TARGETS := ipv4-ssl-cli.out

ALL := $(TARGETS)
$(info $$TARGETS = $(TARGETS))

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $<

%.out: %.o Socket.o
	$(info $$^ = $^)
	$(CXX) $^ Socket.o $(LDFLAGS) -o $^

tcp-cli.out: tcp-cli.o VSocket.o Socket.o SSLSocket.o
	$(CXX) tcp-cli.o VSocket.o Socket.o SSLSocket.o $(LDFLAGS) -o $@

clean:
	@rm -f *.o *.out


