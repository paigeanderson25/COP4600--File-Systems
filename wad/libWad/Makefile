all: libWad.a

Wad.o: Wad.cpp Wad.h
	g++ -std=c++17 -c Wad.cpp -o Wad.o

libWad.a: Wad.o
	ar rcs libWad.a Wad.o

clean:
	rm -f libWad.a Wad.o