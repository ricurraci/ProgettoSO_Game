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
