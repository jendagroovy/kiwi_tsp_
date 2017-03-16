CC=g++
CXX=g++
CXXFLAGS=-std=c++0x

kiwi: main.o
	g++ -o kiwi *.o

clean:
	rm *.o kiwi
