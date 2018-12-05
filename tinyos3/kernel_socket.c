#include "kernel_socket.h"

PortCB* PORT_MAP;

void init_ports()
{
	PORT_MAP = (PortCB*)malloc((MAX_PORT+1)*sizeof(PortCB));
}

int create_pipes(socket_CB* socket1, socket_CB* socket2)
{
	PIPECB* mypipes = (PIPECB*)malloc(2*sizeof(PIPECB));

	(&mypipes[0])->w = 0;
	(&mypipes[0])->r = 0;
	(&mypipes[1])->w = 0;
	(&mypipes[1])->r = 0;

	socket1->reader_pipe = &mypipes[0];
	socket1->writer_pipe = &mypipes[1];
	socket2->reader_pipe = &mypipes[1];
	socket2->writer_pipe = &mypipes[0];

	return 0;
}

int socket_read(void* socket,  char* buf, uint size)
{
	socket_CB* mysocket = (socket_CB*) socket;
	int res = r_pipe_read(mysocket->reader_pipe, buf, size);
	return res;
}

int socket_write(void* socket, const char* buf, uint size)
{

	socket_CB* mysocket = (socket_CB*) socket;
	int res = w_pipe_write(mysocket->writer_pipe, buf, size);
	return res;
}

int socket_close(void* socket)
{
	socket_CB* mysocket = (socket_CB*) socket;
	PortCB* myport = &PORT_MAP[mysocket->port];
	myport->listener = NULL;
	myport->bounded = 0;

	kernel_broadcast(&mysocket->connection);
	//Cond_Signal(&mysocket->connection);

	if(mysocket->reader_pipe!=NULL && mysocket->writer_pipe!=NULL)
	{
		r_pipe_close(mysocket->reader_pipe);
		w_pipe_close(mysocket->writer_pipe);
	}

	free(socket);

	return 1;
}


static file_ops socket_fops = {
	.Read = socket_read,
	.Write = socket_write,
	.Close = socket_close
};


Fid_t sys_Socket(port_t port)
{
	if(port<0 || port>MAX_PORT) goto finish;


	Fid_t* fid=(Fid_t *)malloc(sizeof(Fid_t));
	FCB** fcb=(FCB **)malloc(sizeof(FCB*));

	int res = FCB_reserve(1, fid, fcb);

	if(res != 1) goto finish;

	socket_CB* mysocket = (socket_CB*)malloc(sizeof(socket_CB));
	mysocket->fcb = fcb[0];
	mysocket->port = port;
	mysocket->state = UNBOUND;
	mysocket->connection = COND_INIT;
	//rlnode_init(& mysocket->node, mysocket);
	//rlist_push_back(& PORT_MAP[port]->sockets, & mysocket->node);
	//PORT_MAP[port] = mysocket;
	fcb[0]->streamobj = mysocket;
	fcb[0]->streamfunc = &socket_fops;
	rlnode_init(&mysocket->requests, NULL);

	return fid[0];

	finish:
	return NOFILE;
}

int sys_Listen(Fid_t sock)
{
	if(sock==NOFILE) goto finish;
		//get socket
		PCB* cur = CURPROC;
		//size_t pos =
		FCB* myfcb = cur->FIDT[sock];

	if(myfcb==NULL || !(socket_CB*)myfcb->streamobj) goto finish;

	if(!(socket_CB*) myfcb->streamobj)goto finish;
		socket_CB* mysocket = myfcb->streamobj;

	if(mysocket->port==0 || mysocket->state!=UNBOUND) goto finish;

		//get port of socket
		PortCB* myport = & PORT_MAP[mysocket->port];

		if(myport->bounded==0)	//no listener bounded
		{
			mysocket->state = LISTENER;
			myport->listener = mysocket;
			myport->bounded = 1;
			return 0;
		}

	finish:
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{

	if(lsock==NOFILE) goto finish;

		FCB* myfcb = CURPROC->FIDT[lsock];

	if(myfcb==NULL || !(socket_CB*)myfcb->streamobj) goto finish;

	if(!(socket_CB*) myfcb->streamobj)goto finish;
		socket_CB* mysocket = myfcb->streamobj;

		Fid_t peer = sys_Socket(mysocket->port);
	if(peer==NOFILE) goto finish;

	if(mysocket->state!=LISTENER) goto finish;

		//Cond_Wait(&mutex1,&mysocket->connection);
		kernel_wait(&mysocket->connection,SCHED_USER);

		PortCB* myport = & PORT_MAP[mysocket->port];

	if(myport->bounded==0 || is_rlist_empty(&mysocket->requests))goto finish;

		rlnode* mynode;
		mynode = rlist_pop_front(&mysocket->requests);

		while(mynode->rcb->admitted==2)
		{
			mynode = rlist_pop_front(&mysocket->requests);
		}

		RCB* myrcb = mynode->rcb;

		FCB* myfcb1 = CURPROC->FIDT[peer];
		socket_CB* mysocket1 = myfcb1->streamobj;

		mysocket->peer_fcb = myfcb1;	//listener new peer fcb = new fcb

		mysocket1->state = PEER;
		myrcb->admitted = 1;

		return peer;

	finish:
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	int res = -1;
		if(sock==NOFILE){goto finish;}

		FCB* myfcb = CURPROC->FIDT[sock];
		socket_CB* mysocket = myfcb->streamobj;
		PortCB* myport = & PORT_MAP[port];

	if(myfcb==NULL || mysocket==NULL) {goto finish;}

	if(mysocket->state==LISTENER || myport->bounded==0) {goto finish;}

		socket_CB* listener = myport->listener;
		RCB* request = (RCB*)malloc(sizeof(RCB));

		request->requester = mysocket;
		request->admitted = 0;

		rlnode_init(&request->node, request);

		rlist_push_back(&listener->requests, &request->node);

		kernel_broadcast(&listener->connection);
		//Cond_Signal(&listener->connection);

		kernel_timedwait(&mysocket->connection,SCHED_USER,timeout);
		//Cond_TimedWait(&mutex2,&mysocket->connection, timeout);

		//long int count =100000000;
		//while(count){count--;}

	if(request->admitted!=1)
	{
		request->admitted = 2;
		goto finish;
	}

		mysocket->state = PEER;

		mysocket->peer_fcb = listener->peer_fcb;	//SOS
		socket_CB* mysocket1 = listener->peer_fcb->streamobj;

		mysocket1->peer_fcb = mysocket->fcb;
		listener->peer_fcb = NULL;

		res = create_pipes(mysocket, mysocket1);

		return res;

	finish:
	return res;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	int res = -1;

	FCB* myfcb = CURPROC->FIDT[sock];
	socket_CB* mysocket = myfcb->streamobj;

	if(!mysocket || mysocket->state==UNBOUND || mysocket->state==LISTENER)
		goto finish;


	switch(how)
	{
		case SHUTDOWN_READ:
			res = r_pipe_close(mysocket->reader_pipe);
			break;

		case SHUTDOWN_WRITE:
			res = w_pipe_close(mysocket->writer_pipe);
			break;

		case SHUTDOWN_BOTH:
			res = r_pipe_close(mysocket->reader_pipe);
			res = w_pipe_close(mysocket->writer_pipe);
			break;

		default:
			fprintf(stderr, "shutdown_mode Error\n");
	}
	
	finish:
	return res;
}
