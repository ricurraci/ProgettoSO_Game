CCOPTS= -Wall -g -std=gnu99 -Wstrict-prototypes
LIBS= -lglut -lGLU -lGL -lm -lpthread
CC=gcc
AR=ar


BINS=libso_game.a\
     so_game\
     test_packets_serialization 

OBJS = vec3.o\
       linked_list.o\
       surface.o\
       image.o\
       vehicle.o\
       world.o\
       world_viewer.o\
       so_game_protocol.o\
       so_game_server.o\
       so_game_client.o\
       utils.o\

HEADERS=helpers.h\
	image.h\
	linked_list.h\
	so_game_protocol.h\
	surface.h\
	vec3.h\
	vehicle.h\
	world.h\
	world_viewer.h\
	utils.h\

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

.phony: clean all


all:	$(BINS) 

libso_game.a: $(OBJS) 
	$(AR) -rcs $@ $^
	$(RM) $(OBJS)

so_game: so_game.c libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o $@ $^ $(LIBS)




server: ./so_game_server.c ./libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o ./$@ $^ $(LIBS)

client: ./so_game_client.c ./libso_game.a 
	$(CC) $(CCOPTS) -Ofast -o ./$@ $^ $(LIBS) 

test_server:
	./server ./images/test.pgm ./images/test.ppm ./images/arrow-right.ppm

test_client:
	./client 127.0.0.1




test_packets_serialization: test_packets_serialization.c libso_game.a  
	$(CC) $(CCOPTS) -Ofast -o $@ $^  $(LIBS)

clean:
	rm -rf *.o *~  $(BINS)
