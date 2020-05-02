
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

#include "utils.h"
#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "world_viewer.c"
int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle
int udp_socket;
pthread_t runner_thread;

/**

void keyPressed(unsigned char key, int x, int y)
{
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    vehicle->translational_force_update = 0;
    vehicle->rotational_force_update = 0;
    break;
  case '+':
    viewer.zoom *= 1.1f;
    break;
  case '-':
    viewer.zoom /= 1.1f;
    break;
  case '1':
    viewer.view_type = Inside;
    break;
  case '2':
    viewer.view_type = Outside;
    break;
  case '3':
    viewer.view_type = Global;
    break;
  }
}


void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}


void display(void) {
  WorldViewer_draw(&viewer);
}


void reshape(int width, int height) {
  WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
  World_update(&world);
  usleep(30000);
  glutPostRedisplay();
  
  // decay the commands
  vehicle->translational_force_update *= 0.999;
  vehicle->rotational_force_update *= 0.7;
}
**/

int main(int argc, char **argv) {
  if (argc<3) {
    printf("usage: %s <server_address> <player texture>\n", argv[1]);
    exit(-1);
  }

  printf("loading texture image from %s ... ", argv[2]);
  Image* my_texture = Image_load(argv[2]);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // these come from the server
  int my_id = -1;
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;
  Image* vehicle_texture;
  
  vehicle_texture = get_vehicle_texture(); // Da creare ancora la funzione
  char* buf= (char*)malloc(sizeof(char) *BUFLEN); // cambiare il bufl
  int ret;
  int socket_desc; // rivedere
  struct sockaddr_in server_addr ={0};
  int socketudp;
  
  // TCP socket
  socket_desc = socket(AF_INET, SOCK_STREAM,0);
  ERROR_HELPER(socket_desc,"Could not create a socket");
  
  server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS); // inserire nei define
  server_addr.sin_family = AF_INET; //AF_INET is an address family that is used to designate the type of addresses that your socket can communicate with (in this case, Internet Protocol v4 addresses). When you create a socket, you have to specify its address family, and then you can only use addresses of that type with the socket. The Linux kernel, for example, supports 29 other address families such as UNIX
  server_addr.sin_port= htons(UDP_PORT); // mettere nel define udp port 3000
  
  ret = connect(socket_desc,(struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Could not create connection");
  
  clear(buf);
  IdPacket* id_packet = id_packet_init(GetId, my_id);
  tcp_send(socket_desc, &id_packet->header); // richiesta id 
  ret= tcp_receive(socket_desc,buf); // riceve id
  ERROR_HELPER(ret,"Cannot receive from tcp socket");
  
  Packet_free(&id_packet->header); // liberare l'header del pacchetto per riusarlo per ricevere l'id
  
  id_packet=(IdPacket*)Packet_deserialize(buf, ret); // id letto dal buffer e salvato su idpacket con deserialize
  
  my_id= id_packet->id;
  //if(DEBUG) printf("Id received : &d\n", my_id);
  Packet_free(&id_packet->header);
  
  // richiesta del vehicle texture
  ImagePacket* vehicleTexture_pack;
  if(vehicle_texture) {    // inizializzato prima come image*
      vehicleTexture_pack= image_pack_in(PostTexture, vehicle_texture, my_id);
      tcp_send(socket_desc , &vehicleTexture_pack->header);} // puoi scegliere una tua immagine
      else {
		  vehicleTexture_pack = image_pack_in(GetTexture, NULL, my_id); // immagine di default 
          tcp_send(socket_desc , &vehicleTexture_pack->header);
          ret = tcp_receive(socket_desc, buf);
          ERROR_HELPER(ret, "Cannot receive from tcp socket");
          Packet_free(&vehicleTexture_pack->header);
          vehicleTexture_pack = (ImagePacket*)Packet_deserialize(buf, ret);
          if( (vehicleTexture_pack->header).type != PostTexture || vehicleTexture_pack->id <= 0) {
			fprintf(stderr,"Error: Image corrupted");
			exit(EXIT_FAILURE);			
		}
		///if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
		
		vehicle_texture = vehicleTexture_pack->image;
    }

 // richiesta elevation map
 clear(buf);
 ImagePacket* elevationmap_packet= image_pack_in(GetElevation, NULL, 0);
 tcp_send(socket_desc, &elevationmap_packet->header); // vedere elevationmap
 
 ret = tcp_receive(socket_desc , buf);
 //ERROR_HELPER(ret, "Cannot receive from tcp socket");



 Packet_free(&elevationmap_packet->header);
 elevationmap_packet= (ImagePacket*)Packet_deserialize(buf, ret);
   if (elevationmap_packet->id != 0) {
          fprintf(stderr, "Error: Image problem with id");
          exit(EXIT_FAILURE);
    }
   map_elevation = elevationmap_packet->image; // include verifica

 // richiesta texture superficie
 clear(buf); // ogni volta che cambia pacchetto
 ImagePacket* surftexture_packet = image_packet_init(GetTexture, NULL , 0); // vedere posizione
 tcp_send(socket_desc, &surftexture_packet->header);
 
 ret = tcp_receive(socket_desc, buf);
 ERROR_HELPER(ret, "Cannot receive from tcp socket");
 Packet_free(&surftexture_packet->header); // libero per riutilizarlo
 
 surftexture_packet = (ImagePacket*)Packet_deserialize(buf , ret);
  if ((surftexture_packet->header).type != PostTexture || surftexture_packet->id != 0)  {
      fprintf(stderr, "Error: image corrupted");
      exit(EXIT_FAILURE);
    } //rivedere
 map_texture = surftexture_packet->image;
		  
  
  
  

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, vehicle_texture);
  World_addVehicle(&world, vehicle);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/

 // Parte UDP
 pthread_t connection_checker;

 UpdaterArgs runner_args = {
		.run=1,
		.id = my_id,
		.tcp_desc = socket_desc,
		.texture = vehicle_texture,
		.vehicle = vehicle,
		.world = &world
	}; // aggiunto in utils

	ret = pthread_create(&runner_thread, NULL, updater_thread, &runner_args);
	PTHREAD_ERROR_HELPER(ret, "Error: failed pthread_create runner thread");
		
	ret = pthread_create(&connection_checker, NULL, connection_checker_thread, &runner_args);
	PTHREAD_ERROR_HELPER(ret, "Error: failed pthread_create connection_checker thread");

 
 
       

 
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);
  runner_args.run = 0;
  ret=pthread_join(runner_thread, NULL);
  
  

  // cleanup
  World_destroy(&world);
  ret = close(udp_socket); //cambio var
  ret= close(socket_desc);
  Packet_free(&vehicleTexture_pack->header);
  Packet_free(&elevationmap_packet->header);
  Packet_free(&surftexture_packet->header);
  Vehicle_destroy(vehicle);
  return EXIT_SUCCESS;

}
void *updater_thread(void *args) {
	
	UpdaterArgs* arg = (UpdaterArgs*) args;

	// variabili
	int id = arg->id;
	World *world = arg->world;
	Vehicle *vehicle = arg->vehicle;

	// creazione socket udp
	struct sockaddr_in si_other;
	udp_socket = udp_client_setup(&si_other); // socket.c

    int ret;

	char buffer[BUFLEN];
    
	while(arg->run) {

		float r_f_update = vehicle->rotational_force_update;
		float t_f_update = vehicle->translational_force_update;

		// create vehicle_packet
		VehicleUpdatePacket* vehicle_packet = vehicle_update_init(world, id, r_f_update, t_f_update);
		udp_send(udp_socket, &si_other, &vehicle_packet->header);
		
        clear(buffer); 	// stesso buffer per ricevere
		
		ret = udp_receive(udp_socket, &si_other, buffer);
		WorldUpdatePacket* wu_packet = (WorldUpdatePacket*)Packet_deserialize(buffer, ret);		
		client_update(wu_packet, arg->tcp_desc, world);
 		
		usleep(30000);
	}
	return EXIT_SUCCESS;
}

void *connection_checker_thread(void* args){
	UpdaterArgs* arg = (UpdaterArgs*) args;
	int tcp_desc = arg->tcp_desc;
	int ret;
	char c;
	
	while(1){

		ret = recv(tcp_desc , &c , 1 , MSG_PEEK); // funziona solo se ret recv = 0
		if(ret < 0 && errno == EINTR) continue;
		ERROR_HELPER(ret , "<Errore in connection checker"); 
		
		if(ret == 0) break; // server has quit
		usleep(30000);
	}

	arg->run = 0;	
	
 	ret = pthread_cancel(runner_thread);                                                            // udp non più necessario
	if(ret < 0 && errno != ESRCH) PTHREAD_ERROR_HELPER(ret , "Errore: cancellazione runner_thread non riuscito "); // if errno = ESRCH thread già chiuso
	
	Client_siglePlayerNotification(); // chiusura client , aggiungere funzione modif
	
	ListItem* item = arg->world->vehicles.first;
	
	while(item) {
		Vehicle* v = (Vehicle*)item;
		item = item->next;
		if(v->id != arg->id){
			World_detachVehicle(arg->world , v);
			update_info(arg->world, v->id , 0);
		}			
	}
	
	ret = close(udp_socket);
	if(ret < 0 && errno != EBADF) ERROR_HELPER(ret , "Errore: non posso chiudere udp socket"); // if errno = EBADF socket è già stata chiusa
	
	ret = close(tcp_desc);
	if(ret < 0 && errno != EBADF) ERROR_HELPER(ret , "Errore: non posso chiudere tcp socket");
	
	pthread_exit(EXIT_SUCCESS);


} 
              
void clear(char* buf){
	memset(buf , 0 , BUFLEN * sizeof(char));
}



