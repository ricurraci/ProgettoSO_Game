#include <GL/glut.h> 
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"

#include "utils.h"


World world;
Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

int run_server;  // variabili ausiliarie 
int socket_desc; 
int udp_socket;  


ListHead socket_list;
pthread_t udp_thread;

// FUNZIONI AUSILIARIE

void signal_handler(int sig){
	signal(SIGINT, SIG_DFL); 
	run_server = 0;
	sleep(1);
	
	if(DEBUG){
		fprintf(stdout,"\nServer chiuso\n");
		fflush(stdout);
	}
	
	int ret = pthread_cancel(udp_thread);
	if(ret < 0 && errno != ESRCH) PTHREAD_ERROR_HELPER(ret , "Error: pthread_cancel udp thread failed");
	
	Server_socketClose(&socket_list);   // in utils, funziona con le liste 
	Server_listFree(&socket_list);  // in utils
	
	closeSocket(udp_socket);  // in utils
	closeSocket(socket_desc);
}

void *tcp_client_handler(void *arg){  // funzione che prende i dati dal client

	thread_args* args = (thread_args*)arg;
	
	int socket = args->socket_desc;
	char buf[BUFLEN];   // lungh buffer
    int run = 1;
    int client_id = args->id;    // id del client

    while(run && run_server) {
		
		int ret = tcp_receive(socket , buf);   // funz in utils
		
		if(ret == -1){

			// chiusura del server 
			if(run_server == 0){ 
				run = 0;
				break;
			}
			ERROR_HELPER(ret, "Cannot receive from tcp socket");
		}
		
		else if(!ret) run = 0; // client disconnesso  
		
		else {

			PacketHeader* packet = (PacketHeader*) Packet_deserialize(buf , ret);
			
			switch(packet->type) {  // switcha in base al tipo di pacchetto ricevuto 
				
				// tcp send in utils
				
				case GetId: { 
					IdPacket* id_packet = id_packet_init(GetId, client_id);  // in utils
					tcp_send(socket, &id_packet->header); 
					free(id_packet);
					break;
				}
				
				case GetElevation: {  
					ImagePacket* elevation_packet = image_packet_init(PostElevation, surface_elevation , 0);
					tcp_send(socket, &elevation_packet->header);
					free(elevation_packet);
					break;
				}
				
				case GetTexture: {
					
					if(!((ImagePacket*) packet)->id){  
						ImagePacket* texture_packet = image_packet_init(PostTexture, surface_texture , 0);
						tcp_send(socket, &texture_packet->header);
					}
					else { 
						Vehicle *v = World_getVehicle(&world, ((ImagePacket*) packet)->id);
						ImagePacket* texture_packet = image_packet_init(PostTexture, v->texture, ((ImagePacket*) packet)->id);
						tcp_send(socket, &texture_packet->header);
					}
					break;			
				}
				
				case PostTexture: {
					Vehicle *v = World_getVehicle(&world, ((ImagePacket*) packet)->id);
					v->texture = ((ImagePacket*) packet)->image;	
					break;
				}

				default: break;
			}
		}
	}	

	Vehicle *v = World_getVehicle(&world, args->id); 

	World_detachVehicle(&world, v);
	update_info(&world, args->id, 0);
	Vehicle_destroy(v);
	free(args);
	
	if(run_server) { // if run_server == 0 server sta chiudendo, no detachSocket e close
		Server_detachSocket(&socket_list , socket);  // rimuove la socket dalla lista 
		int ret = close(socket);
		ERROR_HELPER(ret, "Cannot close socket");
    }
    //if(DEBUG) fprintf(stdout,"closing tcp thread...\n");
    	
    pthread_exit(NULL);

}

void *udp_handler(void *arg) {  // funzione che gestisce le azioni del thread in pthread create
	
	// manda e riceve i pacchetti 
	
	struct sockaddr_in udp_client_addr;
	int udp_socket = ((thread_args*)arg)->socket_desc;
	
	int res;
	char buffer[BUFLEN];
	
	while(run_server) {
		res = udp_receive(udp_socket, &udp_client_addr, buffer);   // in utils
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(buffer, res);
		
		world_update(vehicle_packet, &world);
		WorldUpdatePacket* world_packet = world_update_init(&world);		
		
		udp_send(udp_socket, &udp_client_addr, &world_packet->header);  // in utils 
		
	}
	pthread_exit(NULL);
}




int main(int argc, char **argv) {
	
	if (argc<3) {
	printf("usage: %s <elevation_image> <texture_image> <vehicle_texture>\n", argv[1]);
		exit(-1);
	}

	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
	char* vehicle_texture_filename="./images/arrow-right.ppm";
    printf("loading elevation image from %s ... ", elevation_filename);



	struct sockaddr_in si_me;

	// creo le connessioni udp e tcp lato server 
	udp_socket = udp_server_setup(&si_me);   // in utils 
	socket_desc = tcp_server_setup();       // in utils        
	
    // load the images
    surface_elevation = Image_load(elevation_filename);
    if (surface_elevation) {
      printf("Done! \n");
    } else {
      printf("Fail! \n");
    }


    printf("loading texture image from %s ... ", texture_filename);
    surface_texture = Image_load(texture_filename);
    if (surface_texture) {
      printf("Done! \n");
    } else {
      printf("Fail! \n");
    }

    printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
    vehicle_texture = Image_load(vehicle_texture_filename);
    if (vehicle_texture) {
      printf("Done! \n");
    } else {
      printf("Fail! \n");
    }

	
	// creating the world
	World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);
	
	run_server = 1;
	signal(SIGINT,signal_handler);  // funzione ausiliaria
	List_init(&socket_list);
	
	int id = 1;
	int ret, client_desc;
	
	//Creo il thread udp 
	thread_args* udp_args = (thread_args*)malloc(sizeof(thread_args));
	udp_args->socket_desc = udp_socket;
	udp_args->id = -1; 
	
	ret = pthread_create(&udp_thread, NULL, udp_handler, (void*)udp_args); // udp handler ausiliaria
	PTHREAD_ERROR_HELPER(ret, "Cannot create the udp_thread!");

	int sockaddr_len = sizeof(struct sockaddr_in); // accept() la richiede 

	// allocazione dei client 
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));
	
	
	while (run_server) {		
		
		// accetto le connessioni 
		client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);
		if (client_desc == -1 && errno == EINTR) continue; // check interrupt dai signals
		if(client_desc == -1 && run_server == 0) break;    // server in chiusura = 0 
		ERROR_HELPER(client_desc, "Cannot open socket for incoming connection");
				
		// create a vehicle
		Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, vehicle_texture);

		// add it to the world
		World_addVehicle(&world, vehicle);
		
		Server_addSocket(&socket_list , client_desc);  // in utils, aggiunge il client alla lista 

		// creo il thread per i client 
		
		pthread_t client_thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		args->id = id;  // id corrispondente al giocatore client 

		update_info(&world, id, 1);
		
		id++;
		
		ret = pthread_create(&client_thread, NULL, tcp_client_handler, (void*)args);  // tcp client handler 
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		ret = pthread_detach(client_thread); 
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");

	}
	
	//Joining udp thread
	ret = pthread_join(udp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Cannot join the udp_thread!");
	

	// check out the images not needed anymore, stock
	Image_free(surface_elevation);
	Image_free(surface_texture);
	Image_free(vehicle_texture);
	free(client_addr);
	free(udp_args);

	// cleanup
	World_destroy(&world);

}



