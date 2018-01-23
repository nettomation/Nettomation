all: nettomation

HEADERS := $(wildcard *.h)
ENGINEHEADERS := $(wildcard ../../engine/*.h)

nettomation: engine userpages.o
	g++ ../../engine/*.o *.o -std=c++0x -pthread -Wno-write-strings -o nettomation `# -lwiringPi`

%.o: %.cpp $(HEADERS) $(ENGINEHEADERS)
	g++ -c $< -I../.. -std=c++0x -pthread -Wno-write-strings 

engine:
	$(MAKE) -C ../../engine $(MAKECMDGOALS)

clean: engine
	rm -rf nettomation *.o web/*.log *~
