.PHONY : client server all srun crun clean

all: TFTP client server

TFTP:
	g++ -g -c TFTP.cpp -std=gnu++17

client:
	g++ -g -c Cliente.cpp -std=gnu++17
	g++ -g -o ClientSide Cliente.o TFTP.o -Wall -Werror -pthread -lpthread -std=gnu++17

server:
	g++ -g -c Servidor.cpp -std=gnu++17
	g++ -g -o ServerSide Servidor.o TFTP.o -Wall -Werror -pthread -lpthread -std=gnu++17

srun:
	./ServerSide

crun:
	./ClientSide

clean:
	rm ServerSide
	rm ClientSide
