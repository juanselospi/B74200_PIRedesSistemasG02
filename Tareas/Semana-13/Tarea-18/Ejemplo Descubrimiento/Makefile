all: ISFigureInteraction.out

ISFigureInteraction.out: ISFigureInteraction.cc
	g++ -g -pthread ISFigureInteraction.cc -o ISFigureInteraction.out

run: ISFigureInteraction.out
	./ISFigureInteraction.out

clean:
	rm -f *.o *.out

