# Linux Makefile
#

CC  := gcc
CXX := g++
CFLAGS := -g -c
CXXFLAGS := -g -c -std=c++11
LDFLAGS := 
DEPS := Socket.h
TARGETS := ForkMirrorServer.out MirrorClient.out ThreadMirrorServer.out

all: $(TARGETS)
$(info $$TARGETS = $(TARGETS))

%.o: %.cc $(DEPS)
	$(CXX) $(CXXFLAGS) $<

%.out: %.o VSocket.o Socket.o
	$(info $$^ = $^)
	$(CXX) $^ $(LDFLAGS) -o $@

ForkMirrorServer.out:	ForkMirrorServer.o VSocket.o Socket.o 
	$(CXX) ForkMirrorServer.o VSocket.o Socket.o $(LDFLAGS) -o $@
	
ThreadMirrorServer.out:	ThreadMirrorServer.o VSocket.o Socket.o 
	$(CXX) ThreadMirrorServer.o VSocket.o Socket.o $(LDFLAGS) -o $@
	
clean:
	rm -f *.o *.out

