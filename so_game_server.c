
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "so_game_protocol.h"

#define UDP_PORT        3000
#define TCP_PORT        3000
#define MAX_CONN_QUEUE  20
#define BUFLEN          1000000

World world;

Image* surface_texture;
Image* surface_elevation;
Image* vehicle_texture;

int is_running;
int socket_desc;
int udp_socket;

ListHead lista_socket;
pthread_t udp_thread;


int main(int argc, char **argv) {
  	if (argc<3) {
    	printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    	exit(-1);
  	}
  	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
  	char* vehicle_texture_filename="./images/arrow-right.ppm";
  	printf("loading elevation image from %s ... ", elevation_filename);

  	// INIZIALIZZAZIONE UDP SERVER

  	struct sockaddr_in serv_in;
  	int sock = socket(AF_INET, SOCK_DGRAM,0);

  	memset((char *) serv_in, 0, sizeof(*serv_in));

	serv_in->sin_family = AF_INET;
	serv_in->sin_port = htons(UDP_PORT);
	serv_in->sin_addr.s_addr = htonl(INADDR_ANY);

	//binding
	int res = bind(sock , (struct sockaddr*) serv_in, sizeof(*serv_in));
	ERROR_HELPER(res, "Could not bind the udp_socket");

	// FINE SETUP UDP



	// INIZIALIZZAZIONE TCP SERVER

	struct sockaddr_in server_addr={0];
	int sockaddr_len = sizeof(struct sockaddr_in);

	// preparo la socket al listening
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT);

	// crash
	int reuseaddr_opt = 1;

	// operazioni per il socket 
	int ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

	// binding
	ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
	ERROR_HELPER(ret, "Cannot bind address to socket");

	// listening
	ret = listen(socket_desc, MAX_CONN_QUEUE);
	ERROR_HELPER(ret, "Cannot listen on socket");

	// FINE TCP SETUP




	// load the images
	Image* surface_elevation = Image_load(elevation_filename);
	if (surface_elevation) {
	  printf("Done! \n");
	} else {
	  printf("Fail! \n");
	}

  	printf("loading texture image from %s ... ", texture_filename);
	Image* surface_texture = Image_load(texture_filename);
	if (surface_texture) {
	  printf("Done! \n");
	} else {
	  printf("Fail! \n");
	}
	
	printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
	Image* vehicle_texture = Image_load(vehicle_texture_filename);
	if (vehicle_texture) {
	  printf("Done! \n");
	} else {
	  printf("Fail! \n");
	}


	World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);
	
	is_running = 1;

	// gestione segnali
	signal(SIGINT,signal_handler);


	List_init(&lista_socket);

	int id = 1;
	int ret, client_desc;
  
	// creo i thread per udp
	thread_args* udp_args = (thread_args*)malloc(sizeof(thread_args));
	udp_args->socket_desc = sock;
	udp_args->id = -1;

	// UDP_FUNCTION
	ret = pthread_create(&udp_thread, NULL, udp_function, (void*)udp_args);
	PTHREAD_ERROR_HELPER(ret, "Impossibile creare il thread");

	int sockaddr_len = sizeof(struct sockaddr_in);

	// alloco client_addr
	struct sockaddr_in *client_addr = calloc(1, sizeof(struct sockaddr_in));


	while(is_running) {

		// accetto le connessioni dai client multipli
		client_desc = accept(socket_desc, (struct sockaddr *)client_addr, (socklen_t *)&sockaddr_len);

		// gestione errori
		if (client_desc == -1 && errno == EINTR) continue; // check for interruption by signals
		if(client_desc == -1 && run_server == 0) break;    // server is closing
		ERROR_HELPER(client_desc, "Impossibile aprire la socket per le connessioni");

		// gestione veicoli
		Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, vehicle_texture);
		World_addVehicle(&world, vehicle);
		
		add_servsock(&lista_socket , client_desc);	

		pthread_t client_thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		
		// assegnazione dell'id al client
		args->id = id; 	

		update_info(&world, id, 1);
		
		id++;
		
		// TCP_CLIENT
		ret = pthread_create(&client_thread, NULL, tcp_function_client, (void*)args);
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		ret = pthread_detach(client_thread); 
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");



	}

	// GESTIONE DELLA FINE todo









  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);


  
  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);
  
  // WorldViewer_init(&viewer, &world, vehicle);

  
  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);
  return 0;             
}

void signal_handler(int sig){
	int ret1, ret2
	signal(SIGINT, SIG_DFL);
	is_running = 0;
	sleep(1);
	
	if(DEBUG){
		fprintf(stdout,"Chiudo il server...\n");
		fflush(stdout);
	}
	
	int ret = pthread_cancel(udp_thread);
	if(ret < 0 && errno != ESRCH) PTHREAD_ERROR_HELPER(ret , "Errore nella cancellazione del thread udp"); 
	
	Server_socketClose(&lista_socket);
	listfree_serv(&lista_socket);
	
	ret1 = close(udp_socket);
	if ( ret1==-1 ){
		ERROR_HELPER( -1,"Errore nella chiusura della socket");
	}
	ret1 = close(socket_desc);
	if ( ret1==-1 ){
		ERROR_HELPER( -1,"Errore nella chiusura della socket");
	}
	
}

// funzione per udp

void *udp_function(void *arg) {
	
	struct sockaddr_in udp_client_addr;
	int udp_socket = ((thread_args*)arg)->socket_desc;
	
	int ret;
	char buffer[BUFLEN];
	
	while(is_running) {
		ret = udp_receive(udp_socket, &udp_client_addr, buffer);
		VehicleUpdatePacket* vehicle_packet = (VehicleUpdatePacket*)Packet_deserialize(buffer, res);
		
		world_update(vehicle_packet, &world);
		WorldUpdatePacket* world_packet = world_update_init(&world);		
		
		udp_send(udp_socket, &udp_client_addr, &world_packet->header);
		
	}
	pthread_exit(NULL);
}


// funzione per tcp

void *tcp_function_client(void *arg){

	thread_args* args = (thread_args*)arg;
	
	int socket = args->socket_desc;
	char buf[BUFLEN];
    int run = 1;
    int client_id = args->id;

    while(run && is_running) {

		int ret = tcp_receive(socket , buf);

		if(ret == -1){
			if(is_running == 0){ // server sta chiudendo
				run = 0;
				break;
			}
			ERROR_HELPER(ret, "Impossibile ricevere dalla socket tcp");
		}
		
		else if(!ret) run = 0; // client disconnesso
		
		else {

			PacketHeader* packet = (PacketHeader*) Packet_deserialize(buf , ret);
			
			switch(packet->type) {
				
				case GetId: { 
					IdPacket* id_packet = id_packet_init(GetId, client_id);
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
	
	if(is_running) {

		ServerListItem* to_remove = get_servsock(&lista_socket,socket);
		List_detach(&lista_socket, (ListItem*)to_remove);

		int ret = close(socket);
		ERROR_HELPER(ret, "Cannot close socket");
    }
    	
    pthread_exit(NULL);

}

