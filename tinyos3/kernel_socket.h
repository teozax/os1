#ifndef __KERNEL_SOCKET_H
#define __KERNEL_SOCKET_H

#include "kernel_pipe.h"
#include "kernel_proc.h"


typedef enum sid_state_e {
  UNBOUND = 0,
  LISTENER = 1,
  PEER = 2
} socket_state;


typedef struct socket_control_block
{
	FCB* fcb;
  FCB* peer_fcb;
	port_t port;
  socket_state state;
  PIPECB* reader_pipe;
  PIPECB* writer_pipe;
  rlnode requests;
  CondVar connection;

}socket_CB;


typedef struct port_control_block
{
  int bounded;
  socket_CB* listener;
}PortCB;


typedef struct request_control_block
{
  rlnode node;
  socket_CB* requester;
  int admitted;
}RCB;

void init_ports();

int socket_read(void* socket,  char* buf, uint size);
int socket_write(void* socket, const char* buf, uint size);
int socket_close(void* socket);


#endif
