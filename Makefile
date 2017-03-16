CC=g++
CXX=g++
CXXFLAGS=-std=c++0x
LDFLAGS=-pthread

kiwi: main.o
	g++ ${LDFLAGS} -o kiwi *.o

clean:
	rm *.o kiwi
