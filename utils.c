// todo

#include string.h>
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

int add_servsock(ListHead* l, int sock){
	ServerListItem* item = ServerListItem_init(sock);	
	ListItem* result = List_insert(l, l->last, (ListItem*)item);
	return ((ServerListItem*)result)->info;
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

void listFree_serv(ListHead* l){
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

ImagePacket* image_pack_in(Type type, Image *image, int id) {  // temporaneo so_game_protocol.h nell'include, aggiungerlo dopo
      ImagePacket *packet = (ImagePacket*)malloc(sizeof(ImagePacket));
      PacketHeader header;
      header.type= type;
      packet->header = header;
      packet->id = id;
      packet->image = image;
    
      return packet;
}

int udp_client_setup(struct sockaddr_in *si_other) { //temporaneo
	
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    si_other->sin_family = AF_INET;
    si_other->sin_port = htons(UDP_PORT);
    si_other->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);	

	return sock;
}

VehicleUpdatePacket* vehicle_update_init(World *world,int arg_id, float rotational_force, float translational_force) {
    
    VehicleUpdatePacket *vehicle_packet = (VehicleUpdatePacket*) // preso da vehicle default malloc(sizeof(VehicleUpdatePacket));
	PacketHeader v_head;
	v_head.type = VehicleUpdate;

	vehicle_packet->header = v_head;
	vehicle_packet->id = arg_id;
	vehicle_packet->rotational_force = (World_getVehicle(world, arg_id))->rotational_force_update;
	vehicle_packet->translational_force = (World_getVehicle(world, arg_id))->translational_force_update;

	return vehicle_packet;
} // packet.c 

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
}// socket.c

int udp_client_setup(struct sockaddr_in *si_other) {
	
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    si_other->sin_family = AF_INET;
    si_other->sin_port = htons(UDP_PORT);
    si_other->sin_addr.s_addr = inet_addr(SERVER_ADDRESS);	

	return sock;
} // socket.c

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
} // clientkit.c
      

