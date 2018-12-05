#include "kernel_pipe.h"

/*	if((PIPECB*)fcb->streamobj)
	{
		PIPECB* mypipe=(PIPECB*)fcb->streamobj;
		CURPROC->FIDT[mypipe->w_fid]=NULL;
		CURPROC->FIDT[mypipe->r_fid]=NULL;
	}*/

int w_pipe_read()
{
	return -1;
}

int w_pipe_write(void* pipe,  const char* buf, unsigned int size)
{
	int ret=-1;
	PIPECB* mypipe = (PIPECB*)pipe;
	unsigned int count = 0;

	while(count<size)
	{

		if(mypipe->w_fid==-1)
			return -1;
		else if(mypipe->r_fid==-1)
			goto finish;


		if(mypipe->available < PIPE_SIZE){

				if(mypipe->w >= PIPE_SIZE){
						mypipe->w = 0;
				}
				memcpy(mypipe->Buffer+mypipe->w, buf+count, 1);
				mypipe->w++;
				mypipe->available++;
				count++;
				kernel_broadcast(&mypipe->empty);
		}
		else
		{
			kernel_wait(&mypipe->full,SCHED_PIPE);
		}

	}

	ret = count;

	finish:

	return ret;
}

int r_pipe_read(void* pipe,  char* buf, unsigned int size)
{
	PIPECB* mypipe = (PIPECB*)pipe;
	unsigned int count = 0;
	int nextSlot;

	while (count<size)
  {
					if(mypipe->r_fid==-1)
						return -1;
					else	if(mypipe->w_fid==-1 && mypipe->available==0)
						goto finish1;

					if(mypipe->available == 0){
	            kernel_wait(&mypipe->empty, SCHED_PIPE);
	        }
	        nextSlot = mypipe->w - mypipe->available;
	        if(nextSlot < 0){
	            nextSlot += PIPE_SIZE;
	        }
	        //Object nextObj = elements[nextSlot];
					memcpy(buf+count, mypipe->Buffer+nextSlot, 1);
	        mypipe->available--;
	        count++;
					kernel_broadcast(&mypipe->full);
  }

	finish1:

	return count;

}

int w_pipe_close(void* pipe)
{
	PIPECB* mypipe = (PIPECB*)pipe;
	mypipe->w_fid = -1;
	//if(mypipe->r_fid==-1)
		//free(pipe);
	return 1;
}

static file_ops write_pipe_fops = {
  .Read = w_pipe_read,
  .Write = w_pipe_write,
  .Close = w_pipe_close
};



int r_pipe_write()
{
	return -1;
}

int r_pipe_close(void* pipe)
{
	PIPECB* mypipe = (PIPECB*)pipe;
	mypipe->r_fid = -1;
	//if(mypipe->w_fid==-1)
		//free(pipe);
	return 1;
}


static file_ops read_pipe_fops = {
  .Read = r_pipe_read,
  .Write = r_pipe_write,
  .Close = r_pipe_close
};


int sys_Pipe(pipe_t* pipe)
{
	Fid_t *fid=(Fid_t *)malloc(2 * sizeof(Fid_t));
	FCB** fcb=(FCB **)malloc(2 * sizeof(FCB*));

	int res = FCB_reserve(2, fid, fcb);
	if(res != 1) goto finish;

	PIPECB* mypipe = (PIPECB*)malloc(sizeof(PIPECB));
	pipe->read = fid[0];
	pipe->write = fid[1];

	fcb[0]->streamobj = mypipe;
	fcb[0]->streamfunc = &read_pipe_fops;
	fcb[1]->streamobj = mypipe;
	fcb[1]->streamfunc = &write_pipe_fops;

	mypipe->r_fid = fid[0];
	mypipe->w_fid = fid[1];
	mypipe->available = 0;

	mypipe->w = 0;
	mypipe->r = 0;

	finish:
	return res-1;

}
