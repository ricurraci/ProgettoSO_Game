// todo

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "utils.h"
#include "linked_list.h"

// FUNZIONI SERVER 


int udp_server_setup(struct sockaddr_in *si_me) {

	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	ERROR_HELPER(sock, "Could not create the udp_socket");

	
	memset((char *) si_me, 0, sizeof(*si_me));

	si_me->sin_family = AF_INET;
	si_me->sin_port = htons(UDP_PORT);
	si_me->sin_addr.s_addr = htonl(INADDR_ANY);


	//binding
	int res = bind(sock , (struct sockaddr*) si_me, sizeof(*si_me));
	ERROR_HELPER(res, "Could not bind the udp_socket");

	return sock;
}


void udp_send(int socket, struct sockaddr_in *si, const PacketHeader* h) {

	char buffer[BUFLEN];
	char size = 0;

	switch(h->type) {
		case VehicleUpdate:
		{
			VehicleUpdatePacket *vup = (VehicleUpdatePacket*) h;
			size = Packet_serialize(buffer, &vup->header);
			break;
		}
		case WorldUpdate: 
		{
			WorldUpdatePacket *wup = (WorldUpdatePacket*) h;
			size = Packet_serialize(buffer, &wup->header);
			break;
		}
	}
	
	int ret = sendto(socket, buffer, size , 0 , (struct sockaddr *) si, sizeof(*si));
	ERROR_HELPER(ret, "Cannot send message through udp socket");
} // socket.c

int udp_receive(int socket, struct sockaddr_in *si, char *buffer) {

	ssize_t slen = sizeof(*si);

	int ret = recvfrom(socket, buffer, BUFLEN, 0, (struct sockaddr *) si, (socklen_t*) &slen);
	ERROR_HELPER(ret, "Cannot receive from udp socket");

	return ret;
}


int tcp_server_setup(void) {
	
	
	struct sockaddr_in server_addr = {0};

	// serve per accept

	int sockaddr_len = sizeof(struct sockaddr_in); 

	// listening
	int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	server_addr.sin_addr.s_addr = INADDR_ANY; // we want to accept connections from any interface
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(TCP_PORT); // network byte order!

	// We enable SO_REUSEADDR to quickly restart our server after a crash
	int reuseaddr_opt = 1;
	int ret = setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
	ERROR_HELPER(ret, "Cannot set SO_REUSEADDR option");

	// binding alla socket del server
	ret = bind(socket_desc, (struct sockaddr *)&server_addr, sockaddr_len);
	ERROR_HELPER(ret, "Cannot bind address to socket");
	
	// start listening
	ret = listen(socket_desc, MAX_CONN_QUEUE);
	ERROR_HELPER(ret, "Cannot listen on socket");

	return socket_desc;
}




void tcp_send(int socket_desc, PacketHeader* packet){
	int ret;	
	char to_send[BUFLEN];
	char len_to_send[BUFLEN];
	
	int len =  Packet_serialize(to_send, packet);
	snprintf(len_to_send, BUFLEN , "%d", len);
	
	//if(DEBUG) printf("*** Bytes to send : %s\n" , len_to_send);
	
	ret = send(socket_desc, len_to_send, sizeof(long int) , 0);
	ERROR_HELPER(ret, "Impossibile mandare il messaggio");  
	
	while((ret = send(socket_desc, to_send, len , 0)) < 0){
		if(errno == EINTR) continue;
		ERROR_HELPER(ret, "Impossibile mandare il messaggio");  
	}

}




int tcp_receive(int socket_desc , char* msg) {
	
	int ret;
	char len_to_receive[BUFLEN];
	
	ret = recv(socket_desc , len_to_receive , sizeof(long int) , 0);
	ERROR_HELPER(ret, "Cannot receive from tcp socket");
	
	int received_bytes = 0;
	int to_receive = atoi(len_to_receive);
	//if(DEBUG) printf("*** Bytes to_received : %d \n" , to_receive);

	
	while(received_bytes < to_receive){
		ret = recv(socket_desc , msg + received_bytes , to_receive - received_bytes , 0);
		if(ret < 0 && errno == EINTR) continue;
		if(ret < 0) return ret;
		
	    //ERROR_HELPER(ret, "Cannot receive from tcp socket");
	    received_bytes += ret;
	    
	    if(ret==0) break;
	}
	//if(DEBUG) printf("*** Bytes received : %d \n" , ret);

	return received_bytes;
}




void update_info(World *world, int id, int flag) {

	time_t ttime;
	struct tm * timeinfo;

	time ( &ttime );
	timeinfo = localtime ( &ttime );

	char buffer[1024];
	if(flag) {
		sprintf(buffer, "client con id:%d connesso", id);
	} 
	else {
		sprintf(buffer, "client id:%d uscito", id);
	}

	if(DEBUG) {
	fprintf(stdout, "update %s%s\n- numero di veicoli connessi %d ***\n\n", 
			asctime(timeinfo), buffer, world->vehicles.size);
	fflush(stdout);
	}
}


ServerListItem* ServerListItem_init(int sock){
	ServerListItem* item = (ServerListItem*) malloc(sizeof(ServerListItem));
	item->info = sock;
	item->list.prev = NULL;
	item->list.next = NULL;
}

ServerListItem* get_servsock(ListHead* l, int sock){
	ListItem* item = l->first;
	while(item){
		ServerListItem* v=(ServerListItem*)item;
		if(v->info==sock) return v;
		item=item->next;
	}
	return NULL;
}


void Server_detachSocket(ListHead* l, int sock){
	ServerListItem* to_remove = Server_getSocket(l,sock);
	List_detach(l, (ListItem*)to_remove);
}

int Server_addSocket(ListHead* l, int sock){
	ServerListItem* item = ServerListItem_init(sock);	
	ListItem* result = List_insert(l, l->last, (ListItem*)item);
	return ((ServerListItem*)result)->info;
}


void closeSocket(int fd) {
	int ret;
		
	if (fd >= 0) {
	   if (shutdown(fd, SHUT_RDWR) < 0){ // terminate the 'reliable' delivery, SHUT_RDWR makes impossible to receive and send on socket
		 if (errno != ENOTCONN && errno != EINVAL){
			ERROR_HELPER(-1,"Error: shutdown faild");
		 }
	   }
			
	  if (close(fd) < 0) // finally call close()
         ERROR_HELPER( -1,"Error: close socket failed");
   }
}

ServerListItem* Server_getSocket(ListHead* l, int sock){
	ListItem* item = l->first;
	while(item){
		ServerListItem* v=(ServerListItem*)item;
		if(v->info==sock) return v;
		item=item->next;
	}
	return NULL;
}

void Server_listFree(ListHead* l){
	if(l->first == NULL) return;
	ListItem *item = l->first;
	int size = l->size;
	int i;
	for(i=0; i < size; i++) {
		ServerListItem *v = (ServerListItem*) item;
		item = item->next;
		free(v);		
	}
}



void Server_socketClose(ListHead* l){
	if(l == NULL || l->first == NULL) return;
	ListItem* item = l->first;
	int i;
	for(i = 0; i < l->size; i++) {

		ServerListItem* v = (ServerListItem*) item;
		int client_desc = v->info;
		
		//if(DEBUG) fprintf(stdout,"closing socket...%d\n", client_desc);
		//fflush(stdout);
		closeSocket(client_desc);
		item = item->next;
	}
}






// FINE SERVER



void world_update(VehicleUpdatePacket *vehicle_packet, World *world) {
	
	int id = vehicle_packet->id;
		
	Vehicle* v = World_getVehicle(world, id);
	v->rotational_force_update = vehicle_packet->rotational_force;
	v->translational_force_update = vehicle_packet->translational_force; 

	World_update(world);
}


ImagePacket* image_packet_init(Type type, Image *image, int id) {  // temporaneo so_game_protocol.h nell'include, aggiungerlo dopo
      ImagePacket *packet = (ImagePacket*)malloc(sizeof(ImagePacket));
      PacketHeader header;
      header.type= type;
      packet->header = header;
      packet->id = id;
      packet->image = image;
    
      return packet;
}



VehicleUpdatePacket* vehicle_update_init(World *world,int arg_id, float rotational_force, float translational_force) {
    
    VehicleUpdatePacket *vehicle_packet = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
	PacketHeader v_head;
	v_head.type = VehicleUpdate;

	vehicle_packet->header = v_head;
	vehicle_packet->id = arg_id;
	vehicle_packet->rotational_force = (World_getVehicle(world, arg_id))->rotational_force_update;
	vehicle_packet->translational_force = (World_getVehicle(world, arg_id))->translational_force_update;

	return vehicle_packet;
} 



int udp_client_setup(struct sockaddr_in *si_other) {
	
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    si_other->sin_family = AF_INET;
    si_other->sin_port = htons(UDP_PORT);
    si_other->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);	

	return sock;
} 

void client_update(WorldUpdatePacket *deserialized_wu_packet, int socket_desc, World *world) {

	int numb_of_vehicles = deserialized_wu_packet->num_vehicles;
	
	if(numb_of_vehicles > world->vehicles.size) {
		int i;
		for(i=0; i<numb_of_vehicles; i++) {
			int v_id = deserialized_wu_packet->updates[i].id;
			if(World_getVehicle(world, v_id) == NULL) {
	
				char buffer[BUFLEN];

				ImagePacket* vehicle_packet = image_packet_init(GetTexture, NULL, v_id);
    			tcp_send(socket_desc , &vehicle_packet->header);
    			
				char msg[2048];
				sprintf(msg, "./image_%d", v_id);

				int ret = tcp_receive(socket_desc , buffer);
				ERROR_HELPER(ret, "Cannot receive from tcp socket");
    			vehicle_packet = (ImagePacket*) Packet_deserialize(buffer, ret);


				Vehicle *v = (Vehicle*) malloc(sizeof(Vehicle));
				Vehicle_init(v,world, v_id, vehicle_packet->image);
				World_addVehicle(world, v);

				//Image_save(vehicle_packet->image, msg);
				update_info(world, v_id, 1);
				
			} 
		}
	}
	
	else if(numb_of_vehicles < world->vehicles.size) {
		ListItem* item=world->vehicles.first;
		int i, find = 0;
		while(item){
			Vehicle* v = (Vehicle*)item;
			int vehicle_id = v->id;
			for(i=0; i<numb_of_vehicles; i++){
				if(deserialized_wu_packet->updates[i].id == vehicle_id)
					find = 1;
			}

			if (find == 0) {
				World_detachVehicle(world, v);
				update_info(world, vehicle_id, 0);
			}

			find = 0;
			item=item->next;
		}
	}

	int i;
	for(i = 0 ; i < world->vehicles.size ; i++) {
		Vehicle *v = World_getVehicle(world, deserialized_wu_packet->updates[i].id);
		
		v->x = deserialized_wu_packet->updates[i].x;
		v->y = deserialized_wu_packet->updates[i].y;
		v->theta = deserialized_wu_packet->updates[i].theta;
	}
} 
     

Image* get_vehicle_texture() {

	Image* my_texture;
	char image_path[256];
	
	fprintf(stdout, "\nOPERATING SYSTEM PROJECT 2020 - CLIENT SIDE ***\n\n"); // modificare dopo
	fflush(stdout);
	fprintf(stdout, "\nWelcome!\nThanks for joining, you will be soon connected to the game server.\n");
	fprintf(stdout, "First, you can choose to use your own image. Only .ppm images are supported.\n");
	
	while(1){
		fprintf(stdout, "Insert path ('no' for default vehicle image) ('q' to exit) :\n");
		if(scanf("%s",image_path) < 0){
			fprintf(stderr, "Error occured!\n");
			exit(EXIT_FAILURE);
		}
		if(strcmp(image_path, "q") == 0) exit(EXIT_SUCCESS);
		if(strcmp(image_path, "no") == 0) return NULL;
		else {
			char *dot = strrchr(image_path, '.');
			if (dot == NULL || strcmp(dot, ".ppm")!=0){
				fprintf(stderr,"Sorry! Image not found or not supported... \n");
			}
			else{
				my_texture = Image_load(image_path);
				if (my_texture) {
					printf("Done! \n");
					return my_texture;
				} else {
					fprintf(stderr,"Sorry! Chose image cannot be loaded... \n");
					exit(EXIT_FAILURE);
				}
			}
		}
		usleep(3000);
	}
	return NULL; // will never be reached
}




void Client_siglePlayerNotification(void){
	fprintf(stdout, "\n\nConnection with SEREVR ip:[%s] port:[%d] ENDED\n", 
				SERVER_ADDRESS , TCP_PORT);
	fprintf(stdout,"\nMultiplayer no longer avaible!! ***\n\n");
	fflush(stdout);
}

int tcp_client_setup(void){
	int ret;

	// variables for handling a socket
	int socket_desc;
	struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

	// create a socket
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	ERROR_HELPER(socket_desc, "Could not create socket");

	// set up parameters for the connection
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDRESS);
	server_addr.sin_family      = AF_INET;
	server_addr.sin_port        = htons(TCP_PORT); // network byte order!

	// initiate a connection on the socket
	ret = connect(socket_desc, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	ERROR_HELPER(ret, "Could not create connection");

	//if (DEBUG) fprintf(stderr, "Connection established!\n");  
	
	return socket_desc;
}

IdPacket* id_packet_init(Type header_type, int id){
	PacketHeader id_header;
	id_header.type = header_type;
	
	IdPacket* id_packet = (IdPacket*)malloc(sizeof(IdPacket));
	id_packet->header = id_header;
	id_packet->id = id;	
	return id_packet;
}

