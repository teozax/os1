#ifndef __KERNEL_PIPE_H
#define __KERNEL_PIPE_H

#include "kernel_cc.h"
#include "kernel_streams.h"
#include "tinyos.h"


#define PIPE_SIZE 8192

typedef struct pipe_control_block{
	char Buffer[PIPE_SIZE];
	int w, r, available;
	FCB* reader;
	FCB* writer;
	Fid_t r_fid;
	Fid_t w_fid;
	CondVar empty, full;
}PIPECB;

int w_pipe_read();
int w_pipe_write(void* pipe,  const char* buf, unsigned int size);
int w_pipe_close(void* pipe);

int r_pipe_write();
int r_pipe_close(void* pipe);
int r_pipe_read(void* pipe,  char* buf, unsigned int size);

#endif
