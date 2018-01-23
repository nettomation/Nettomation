all: engine

HEADERS := $(wildcard *.h)

engine: autoregister.o encoder.o main.o smartlock.o webcontent.o webtop.o dispatcher.o genericthread.o renderingrecord.o watchdogthread.o webthread.o sha256.o

%.o: %.cpp $(HEADERS)
	g++ -c $< -std=c++0x -pthread -Wno-write-strings

clean:
	rm -rf *.o *~
