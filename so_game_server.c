
#include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <signal.h>

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



void signal_handler(int sig){
	int ret1, ret2;
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
	listFree_serv(&lista_socket);
	
	ret1 = close(udp_socket);
	if ( ret1==-1 ){
		ERROR_HELPER( -1,"Errore nella chiusura della socket");
	}
	ret1 = close(socket_desc);
	if ( ret1==-1 ){
		ERROR_HELPER( -1,"Errore nella chiusura della socket");
	}
	
}




int main(int argc, char **argv) {

	if (argc<3) {
    	printf("usage: %s <elevation_image> <texture_image>\n", argv[1]);
    	exit(-1);
  	}
  	char* elevation_filename=argv[1];
	char* texture_filename=argv[2];
  	char* vehicle_texture_filename="./images/arrow-right.ppm";
  	printf("loading elevation image from %s ... ", elevation_filename);

  	struct sockaddr_in si_me;
  	udp_socket = udp_server_setup(&si_me);

  	/*INIZIALIZZAZIONE UDP SERVER

  	struct sockaddr_in serv_in;
  	int sock = socket(AF_INET, SOCK_DGRAM,0);

  	//memset((char *) serv_in, 0, sizeof(*serv_in));

	serv_in->sin_family = AF_INET;
	serv_in->sin_port = htons(UDP_PORT);
	serv_in->sin_addr.s_addr = htonl(INADDR_ANY);

	//binding
	int res = bind(sock , (struct sockaddr*) serv_in, sizeof(*serv_in));
	ERROR_HELPER(res, "Could not bind the udp_socket");

	FINE SETUP UDP */ 



	//INIZIALIZZAZIONE TCP SERVER

	struct sockaddr_in server_addr={0};
	int tcp_sockaddr_len = sizeof(struct sockaddr_in);

	// preparo la socket al listening
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT);

	// crash
	int reuseaddr_opt = 1;

	// operazioni per il socket 
	int tcp_ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(tcp_ret, "Cannot set SO_REUSEADDR option");

	// binding
	tcp_ret = bind(socket_desc, (struct sockaddr *)&server_addr, tcp_sockaddr_len);
	ERROR_HELPER(tcp_ret, "Cannot bind address to socket");

	// listening
	tcp_ret = listen(socket_desc, MAX_CONN_QUEUE);
	ERROR_HELPER(tcp_ret, "Cannot listen on socket");

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

	// construct the world
	World_init(&world, surface_elevation, surface_texture, 0.5, 0.5, 0.5);
	
	is_running = 1;

	// gestione segnali
	signal(SIGINT,signal_handler);


	List_init(&lista_socket);

	int id = 1;
	int ret, client_desc;
  
	// creo i thread per udp
	thread_args* udp_args = (thread_args*)malloc(sizeof(thread_args));
	udp_args->socket_desc = udp_socket;
	udp_args->id = -1;

	// UDP_FUNCTION fuori il main, gestisce i pacchetti udp
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
		if(client_desc == -1 && is_running == 0) break;    // server is closing
		ERROR_HELPER(client_desc, "Impossibile aprire la socket per le connessioni");

		// gestione veicoli
		Vehicle *vehicle=(Vehicle*) malloc(sizeof(Vehicle));
		Vehicle_init(vehicle, &world, id, vehicle_texture);
		World_addVehicle(&world, vehicle);
		
		// funzione nelle utils
		add_servsock(&lista_socket , client_desc);	

		pthread_t client_thread;
		thread_args* args = (thread_args*)malloc(sizeof(thread_args));
		args->socket_desc = client_desc;
		
		// assegnazione dell'id al client
		args->id = id; 	

		// funzione nelle utils, si occupa di aggiornare la lista di veicoli connessi
		update_info(&world, id, 1);
		
		id++;
		
		// TCP_CLIENT funzione fuori dal main
		ret = pthread_create(&client_thread, NULL, tcp_function_client, (void*)args);
		PTHREAD_ERROR_HELPER(ret, "Could not create a new thread");

		ret = pthread_detach(client_thread); 
	    PTHREAD_ERROR_HELPER(ret, "Could not detach the thread");



	}

	// GESTIONE DELLA FINE

	ret = pthread_join(udp_thread, NULL);
	PTHREAD_ERROR_HELPER(ret, "Impossibile fare il join");
	
	// cleanup

	World_destroy(&world);
	Image_free(surface_elevation);
	Image_free(surface_texture);
	Image_free(vehicle_texture);
	free(client_addr);
	free(udp_args);







  // not needed here
  
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


}

