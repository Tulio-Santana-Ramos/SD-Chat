.PHONY : client server all srun crun clean

all: TFTP client server

TFTP:
	g++ -g -c TFTP.cpp

client:
	g++ -g -c Cliente.cpp
	g++ -g -o ClientSide Cliente.o TFTP.o -Wall -Werror -pthread -lpthread

server:
	g++ -g -c Servidor.cpp
	g++ -g -o ServerSide Servidor.o TFTP.o -Wall -Werror -pthread -lpthread

srun:
	./ServerSide

crun:
	./ClientSide

clean:
	rm ServerSide
	rm ClientSide