SFML_PATH=/usr/local/SFML-2.2/
CXXFLAGS= -std=c++14 -Wall -Wpedantic -I${SFML_PATH}include/
LDFLAGS=-L${SFML_PATH}lib/
CFLAGS=-g -lsfml-graphics -lsfml-window -lsfml-system -lsfml-network -ljpeg -pthread
CPPFLAGS=
LDLIBS=
LIBS=
CPP=g++

all: client #server

client: main.o
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(SFML_PATH)/lib/:lib $(CPP) $(CXXFLAGS) $(LDFLAGS) $(LIBS) -Llib $^ -o $@ $(CFLAGS)

run: client
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(SFML_PATH)/lib/:lib ./client
	

clean:
	\rm -f *.o client
