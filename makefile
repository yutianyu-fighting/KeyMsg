#LIBFLAG=-L$(HOME)/lib

LIBS= -lpthread -litcastsocket -lmessagereal -lmysqlclient
LIBSPATH=-L./lib

INC=-I./include

serv=./bin/keymngserver
client=./bin/keymngclient

target=$(serv) $(client)

ALL:$(target)

$(serv):./src/keymngserver.o ./src/keymngserverop.o ./src/keymnglog.o ./src/myipc_shm.o ./src/keymng_shmop.o ./src/deal_mysql.o ./src/base64.o
	gcc $^ -o $@ $(LIBSPATH) $(LIBS) 

$(client):./src/keymngclient.o  ./src/keymnglog.o  ./src/keymngclientop.o  ./src/myipc_shm.o ./src/keymng_shmop.o
	gcc $^ -o $@ $(LIBSPATH) $(LIBS)

%.o:%.c
	gcc -c $< -o $@ $(INC) 

.PHONY:clean ALL

clean:
	rm -f ./src/*.o $(target)
