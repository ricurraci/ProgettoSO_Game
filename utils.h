// todo
#ifndef _UTILS_H_
#define _UTILS_H_

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"

// SERVER UTILITY

typedef struct thread_args {
	int id;
	int socket_desc;	
} thread_args;

typedef struct ServerListItem{
  ListItem list;
  int info;
} ServerListItem;

int add_servsock(ListHead* l, int sock);

void listFree_serv(ListHead* l);

void update_info(World *world, int id, int flag);

void signal_handler(int sig);

void *udp_function(void *arg);

void *tcp_function_client(void *arg);

int udp_server_setup(struct sockaddr_in *si_me);

ServerListItem* get_servsock(ListHead* l, int sock);

void tcp_send(int socket_desc, PacketHeader* packet);

int tcp_receive(int socket_desc , char* msg);




#endif