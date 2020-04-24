// todo
#ifndef _UTILS_H_
#define _UTILS_H_

#include "../image/image.h"
#include "../surface/surface.h"
#include "../world/world.h"
#include "../vehicle/vehicle.h"
#include "../world/world_viewer.h"
#include "../protocol/so_game_protocol.h"
#include "../packet/packet.h"
#include "../socket/socket.h"
#include "../common/common.h"

// SERVER UTILITY

int add_servsock(ListHead* l, int sock);

void listFree_serv(ListHead* l);

void update_info(World *world, int id, int flag)


#endif