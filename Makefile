.PHONY : client server all srun crun clean

all: client server

client:
	g++ -g -c Cliente.cpp
	g++ -g -o ClientSide Cliente.o -Wall -Werror -pthread -lpthread
	rm Cliente.o

server:
	g++ -g -c Servidor.cpp
	g++ -g -o ServerSide Servidor.o -Wall -Werror -pthread -lpthread
	rm Servidor.o

srun:
	./ServerSide

crun:
	./ClientSide

clean:
	rm ServerSide
	rm ClientSide