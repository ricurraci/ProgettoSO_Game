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
#define SERVER_ADDRESS    "127.0.0.1"
#define UDP_SOCKET_NAME   "[UDP]"
#define TCP_SOCKET_NAME   "[TCP]"
#define UDP_PORT        3000
#define TCP_PORT        3000
#define MAX_CONN_QUEUE  20
#define UDP_BUFLEN      512
#define DEBUG           1   
#define BUFLEN          1000000
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

typedef struct {
  volatile int run;
  int id;
  int tcp_desc;
  Image *texture;
  Vehicle *vehicle;
  World *world;
} UpdaterArgs;

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

ImagePacket* image_pack_in(Type type, Image *image, int id);

int udp_client_setup(struct sockaddr_in *si_other);

VehicleUpdatePacket* vehicle_update_init(World *world,int arg_id , float rotational_force , float translational_force);

void udp_send(int socket, struct sockaddr_in *si, const PacketHeader* h);

int udp_receive(int socket, struct sockaddr_in *si, char *buffer);

int udp_client_setup(struct sockaddr_in *si_other);

void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world);

Image* get_vehicle_texture();

IdPacket* id_packet_init(Type header_type, int id);

ImagePacket* image_packet_init(Type type, Image *image, int id);

void *updater_thread(void *args);

void *connection_checker_thread(void* args);

void Client_siglePlayerNotification(void);

void clear(char* buf);




#endif
