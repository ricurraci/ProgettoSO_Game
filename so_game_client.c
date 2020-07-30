
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

//<player texture> nel print


int main(int argc, char **argv) {
  if (argc<2) {
    printf("usage: %s <server_address>\n", argv[1]);
    exit(-1);
  }




 
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // dal server
  int my_id = -1;
  Image* map_elevation; //image.h
  Image* map_texture;
  //Image* my_texture_from_server;
  Image* vehicle_texture;
  
  vehicle_texture = get_vehicle_texture(); // Funzione carica texture del veicolo e presentazione progetto
  char* buf= (char*)malloc(sizeof(char) *BUFLEN); 
// TCP socket 
  int socket_desc = tcp_client_setup();
  
  World world;
  Vehicle* vehicle;

  int ret;
  
 
 // gets ID from the server (TCP)
  
  clear(buf);
  IdPacket* id_packet = id_packet_init(GetId, my_id);
  
  tcp_send(socket_desc, &id_packet->header); // richiesta id utils tcpsend
  ret= tcp_receive(socket_desc,buf); // riceve id utils tcpreceive
  ERROR_HELPER(ret,"Impossibile ricevere da tcp socket");
  
  Packet_free(&id_packet->header); // liberare l'header del pacchetto per riusarlo per ricevere l'id
  
  id_packet=(IdPacket*)Packet_deserialize(buf, ret); // id letto dal buffer e salvato su idpacket con deserialize
  
  my_id= id_packet->id;
  //if(DEBUG) printf("Id received : &d\n", my_id);
  //printf("prova");
  Packet_free(&id_packet->header);
  clear(buf);
  // richiesta del vehicle texture
  
  ImagePacket* vehicleTexture_packet; // imagepacket in protocol.h
  
  if(vehicle_texture) {
    vehicleTexture_packet = image_packet_init(PostTexture, vehicle_texture, my_id);    // client sceglie di utilizzare una sua immagine
    tcp_send(socket_desc , &vehicleTexture_packet->header);
  } else {
    vehicleTexture_packet = image_packet_init(GetTexture, NULL, my_id); //client sceglie immagine di default
    tcp_send(socket_desc , &vehicleTexture_packet->header); 
    ret = tcp_receive(socket_desc , buf);
    ERROR_HELPER(ret, "Impossibile ricevere da tcp socket");
    
    Packet_free(&vehicleTexture_packet->header); // libera per utilizzare stesso vehicletexturepack per ricevere
    vehicleTexture_packet = (ImagePacket*)Packet_deserialize(buf, ret);
    
    if( (vehicleTexture_packet->header).type != PostTexture || vehicleTexture_packet->id <= 0) {
      fprintf(stderr,"Error: Image corrupted");
      exit(EXIT_FAILURE);     
    }
    ///if(DEBUG) printf("%s VEHICLE TEXTURE RECEIVED FROM SERVER\n", TCP_SOCKET_NAME);
    
    vehicle_texture = vehicleTexture_packet->image;
    }

 // richiesta elevation map
 clear(buf);
 //printf("prova 2");
 ImagePacket* elevationmap_packet= image_packet_init(GetElevation, NULL, 0);// vedere in utils
 tcp_send(socket_desc, &elevationmap_packet->header); // vedere elevationmap
 
 ret = tcp_receive(socket_desc , buf);
 ERROR_HELPER(ret, "Impossibile ricevere da tcp socket");

  Packet_free(&elevationmap_packet->header); // libera per utilizzare stesso elevionmappacket(imagepacket) per ricevere
  elevationmap_packet = (ImagePacket*)Packet_deserialize(buf, ret);
    
    if( (elevationmap_packet->header).type != PostElevation || elevationmap_packet->id != 0) {
    fprintf(stderr,"Error: Image corrupted");
    exit(EXIT_FAILURE);   
  }
  
   map_elevation = elevationmap_packet->image; // include verifica

 // richiesta texture superficie
 clear(buf); // ogni volta che cambia pacchetto
 ImagePacket* surftexture_packet = image_packet_init(GetTexture, NULL , 0); // vedere posizione
 tcp_send(socket_desc, &surftexture_packet->header);
 
 ret = tcp_receive(socket_desc, buf);
 ERROR_HELPER(ret, "Impossibile ricevere da tcp socket");

 Packet_free(&surftexture_packet->header); // libero per riutilizarlo
 surftexture_packet = (ImagePacket*)Packet_deserialize(buf , ret);
 
  if ((surftexture_packet->header).type != PostTexture || surftexture_packet->id != 0)  {
      fprintf(stderr, "Error: image corrupted");
      exit(EXIT_FAILURE);
    } 
  
  map_texture = surftexture_packet->image;
		  
  
  
  

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5); // tutto in world.c
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, my_id, vehicle_texture);
  World_addVehicle(&world, vehicle);



 // Parte UDP
 pthread_t connection_checker;

 UpdaterArgs runner_args = {
		.run=1,
		.id = my_id,
		.tcp_desc = socket_desc,
		.texture = vehicle_texture,
		.vehicle = vehicle,
		.world = &world
	}; // aggiunto in utils struct

	ret = pthread_create(&runner_thread, NULL, updater_thread, &runner_args); // update periodici
	PTHREAD_ERROR_HELPER(ret, "Error: failed pthread_create runner thread");
		
	ret = pthread_create(&connection_checker, NULL, connection_checker_thread, &runner_args); // controllo connessione periodica
	PTHREAD_ERROR_HELPER(ret, "Error: failed pthread_create connection_checker thread");

 
 
       

 
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);
  runner_args.run = 0;
  ret=pthread_join(runner_thread, NULL);
  
  

  // cleanup
  World_destroy(&world);
  ret = close(udp_socket); 
  ret= close(socket_desc);
  Packet_free(&vehicleTexture_packet->header);
  Packet_free(&elevationmap_packet->header);
  Packet_free(&surftexture_packet->header);
  Vehicle_destroy(vehicle);
  return EXIT_SUCCESS;

}
void *updater_thread(void *args) { // il server riceve(udp) aggiornamenti periodici dal client 
	
	UpdaterArgs* arg = (UpdaterArgs*) args;

	// variabili
	int id = arg->id;
	World *world = arg->world;
	Vehicle *vehicle = arg->vehicle;

	// creazione socket udp
	struct sockaddr_in si_other;
	udp_socket = udp_client_setup(&si_other); // vedere utils

    int ret;

	char buffer[BUFLEN];
    
	while(arg->run) {

		float r_f_update = vehicle->rotational_force_update;
		float t_f_update = vehicle->translational_force_update;

		// crea vehicle_packet
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
 
void *connection_checker_thread(void* args){ // controlla connessione con server, se non c'è più connessione rimuove veicolo dal mondo
	UpdaterArgs* arg = (UpdaterArgs*) args;
	int tcp_desc = arg->tcp_desc;
	int ret;
	char c;
	
	while(1){

		ret = recv(tcp_desc , &c , 1 , MSG_PEEK); // funziona solo se ret recv = 0
		if(ret < 0 && errno == EINTR) continue;
		ERROR_HELPER(ret , "<Errore  connection checker"); 
		
		if(ret == 0) break; // server chiuso
		usleep(30000);
	}

	arg->run = 0;	
	
 	ret = pthread_cancel(runner_thread);                                                            // udp non più necessario
	if(ret < 0 && errno != ESRCH) PTHREAD_ERROR_HELPER(ret , "Errore: cancellazione runner_thread non riuscito "); // if errno = ESRCH thread già chiuso
	
	Client_siglePlayerNotification(); // chiusura client , aggiungere funzione modif in utils
	
	ListItem* item = arg->world->vehicles.first;
	
	while(item) {
		Vehicle* v = (Vehicle*)item;
		item = item->next;
		if(v->id != arg->id){
			World_detachVehicle(arg->world , v);
			update_info(arg->world, v->id , 0); // utils
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




