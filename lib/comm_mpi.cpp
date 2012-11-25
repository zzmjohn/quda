#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>
#include <comm_quda.h>
#include <quda_internal.h>
#include <face_quda.h>

static char hostname[128] = "undetermined";
static int fwd_nbr=-1;
static int back_nbr=-1;
static int rank = 0;
static int size = -1;
static int num_nodes;
extern int getGpuCount();
static int which_gpu = -1;

static int x_fwd_nbr=-1;
static int y_fwd_nbr=-1;
static int z_fwd_nbr=-1;
static int t_fwd_nbr=-1;
static int x_back_nbr=-1;
static int y_back_nbr=-1;
static int z_back_nbr=-1;
static int t_back_nbr=-1;

static int xgridsize=1;
static int ygridsize=1;
static int zgridsize=1;
static int tgridsize=1;
static int xgridid = -1;
static int ygridid = -1;
static int zgridid = -1;
static int tgridid = -1;

static int manual_set_partition[4] ={0, 0, 0, 0};

#define X_FASTEST_DIM_NODE_RANKING

void
comm_set_gridsize(const int *X, int nDim)
{
  if (nDim != 4) errorQuda("Comms dimensions %d != 4", nDim);

  xgridsize = X[0];
  ygridsize = X[1];
  zgridsize = X[2];
  tgridsize = X[3];

  int volume = 1;
  for (int i=0; i<nDim; i++) volume *= X[i];

  int size = -1;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (volume != size)
    errorQuda("Number of processes %d must match requested MPI volume %d",
	      size, volume);

  return;
}

/* This function is for and testing debugging purpose only The
 * partitioning schedume should be generated automatically in
 * production runs. Don't use this function if you don't know what you
 * are doing
 */
void
comm_dim_partitioned_set(int dir)
{
  manual_set_partition[dir] = 1;
  return;
}


int 
comm_dim_partitioned(int dir)
{
  int ret = 0;
  
  switch(dir){
  case 0: 
    ret = (xgridsize > 1);    
    break;
  case 1: 
    ret = (ygridsize > 1);
    break;
  case 2: 
    ret = (zgridsize > 1);
    break;
  case 3: 
    ret = (tgridsize > 1);
    break;    
  default:
    printf("ERROR: invalid direction\n");
    comm_exit(1);
  }
  
  if( manual_set_partition[dir]){
    ret = manual_set_partition[dir];
  }
  
  return ret;
}

static void
comm_partition(void)
{
  /*
  printf("xgridsize=%d\n", xgridsize);
  printf("ygridsize=%d\n", ygridsize);
  printf("zgridsize=%d\n", zgridsize);
  printf("tgridsize=%d\n", tgridsize);
  */
  if(xgridsize*ygridsize*zgridsize*tgridsize != size){
    if (rank ==0){
      printf("ERROR: Invalid configuration (t,z,y,x gridsize=%d %d %d %d) "
             "but # of MPI processes is %d\n", tgridsize, zgridsize, ygridsize, xgridsize, size);
    }
    comm_exit(1);
  }

  int leftover;

#ifdef X_FASTEST_DIM_NODE_RANKING
  tgridid  = rank/(zgridsize*ygridsize*xgridsize);
  leftover = rank%(zgridsize*ygridsize*xgridsize);
  zgridid  = leftover/(ygridsize*xgridsize);
  leftover = leftover%(ygridsize*xgridsize);
  ygridid  = leftover/xgridsize;
  xgridid  = leftover%xgridsize;
  #define GRID_ID(xid,yid,zid,tid) (tid*zgridsize*ygridsize*xgridsize+zid*ygridsize*xgridsize+yid*xgridsize+xid)
#else
  xgridid  = rank/(ygridsize*zgridsize*tgridsize);
  leftover = rank%(ygridsize*zgridsize*tgridsize);
  ygridid  = leftover/(zgridsize*tgridsize);
  leftover = leftover%(zgridsize*tgridsize);
  zgridid  = leftover/tgridsize;
  tgridid  = leftover%tgridsize;  
#define GRID_ID(xid,yid,zid,tid) (xid*ygridsize*zgridsize*tgridsize+yid*zgridsize*tgridsize+zid*tgridsize+tid)
#endif

  if (getVerbosity() >= QUDA_DEBUG_VERBOSE)
    printf("My rank: %d, gridid(t,z,y,x): %d %d %d %d\n", rank, tgridid, zgridid, ygridid, xgridid);


  int xid, yid, zid, tid;
  //X direction neighbors
  yid =ygridid;
  zid =zgridid;
  tid =tgridid;
  xid=(xgridid +1)%xgridsize;
  x_fwd_nbr = GRID_ID(xid,yid,zid,tid);
  xid=(xgridid -1+xgridsize)%xgridsize;
  x_back_nbr = GRID_ID(xid,yid,zid,tid);

  //Y direction neighbors
  xid =xgridid;
  zid =zgridid;
  tid =tgridid;
  yid =(ygridid+1)%ygridsize;
  y_fwd_nbr = GRID_ID(xid,yid,zid,tid);
  yid=(ygridid -1+ygridsize)%ygridsize;
  y_back_nbr = GRID_ID(xid,yid,zid,tid);

  //Z direction neighbors
  xid =xgridid;
  yid =ygridid;
  tid =tgridid;
  zid =(zgridid+1)%zgridsize;
  z_fwd_nbr = GRID_ID(xid,yid,zid,tid);
  zid=(zgridid -1+zgridsize)%zgridsize;
  z_back_nbr = GRID_ID(xid,yid,zid,tid);

  //T direction neighbors
  xid =xgridid;
  yid =ygridid;
  zid =zgridid;
  tid =(tgridid+1)%tgridsize;
  t_fwd_nbr = GRID_ID(xid,yid,zid,tid);
  tid=(tgridid -1+tgridsize)%tgridsize;
  t_back_nbr = GRID_ID(xid,yid,zid,tid);

  if (getVerbosity() >= QUDA_DEBUG_VERBOSE) {
    printf("MPI rank: rank=%d, hostname=%s, x_fwd_nbr=%d, x_back_nbr=%d\n", rank, comm_hostname(), x_fwd_nbr, x_back_nbr);
    printf("MPI rank: rank=%d, hostname=%s, y_fwd_nbr=%d, y_back_nbr=%d\n", rank, comm_hostname(), y_fwd_nbr, y_back_nbr);
    printf("MPI rank: rank=%d, hostname=%s, z_fwd_nbr=%d, z_back_nbr=%d\n", rank, comm_hostname(), z_fwd_nbr, z_back_nbr);
    printf("MPI rank: rank=%d, hostname=%s, t_fwd_nbr=%d, t_back_nbr=%d\n", rank, comm_hostname(), t_fwd_nbr, t_back_nbr);
  }
}

int 
comm_get_neighbor_rank(int dx, int dy, int dz, int dt)
{
  int ret;
#ifdef X_FASTEST_DIM_NODE_RANKING
#define GRID_ID(xid,yid,zid,tid) (tid*zgridsize*ygridsize*xgridsize+zid*ygridsize*xgridsize+yid*xgridsize+xid)
#else
#define GRID_ID(xid,yid,zid,tid) (xid*ygridsize*zgridsize*tgridsize+yid*zgridsize*tgridsize+zid*tgridsize+tid)
#endif

  
  int xid, yid, zid, tid;
  xid=(xgridid + dx + xgridsize)%xgridsize;
  yid=(ygridid + dy + ygridsize)%ygridsize;
  zid=(zgridid + dz + zgridsize)%zgridsize;
  tid=(tgridid + dt + tgridsize)%tgridsize;

  ret = GRID_ID(xid,yid,zid,tid);

  return ret;
}

void comm_create(int argc, char **argv)
{
  MPI_Init (&argc, &argv);  
}

void 
comm_init(void)
{
  int i;
  
  static int firsttime=1;
  if (!firsttime) return;
  firsttime = 0;

  gethostname(hostname, 128);
  hostname[127] = '\0';

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int gpus_per_node = getGpuCount();  

  comm_partition();

  back_nbr = (rank -1 + size)%size;
  fwd_nbr = (rank +1)%size;
  num_nodes=size / getGpuCount();
  if(num_nodes ==0) {
	num_nodes=1;
  }

  //determine which gpu this MPI process is going to use
  char* hostname_recv_buf = (char*)safe_malloc(128*size);
  
  int rc = MPI_Allgather(hostname, 128, MPI_CHAR, hostname_recv_buf, 128, MPI_CHAR, MPI_COMM_WORLD);
  if (rc != MPI_SUCCESS){
    printf("ERROR: MPI_Allgather failed for hostname\n");
    comm_exit(1);
  }

  which_gpu=0;
  for(i=0;i < size; i++){
    if (i == rank){
      break;
    }
    if (strncmp(hostname, hostname_recv_buf + 128*i, 128) == 0){
      which_gpu ++;
    }
  }
  
  if (which_gpu >= gpus_per_node){
    printf("ERROR: invalid gpu(%d) to use in rank=%d mpi process\n", which_gpu, rank);
    comm_exit(1);
  }
  
  srand(rank*999);
  
  host_free(hostname_recv_buf);
  return;
}

char *
comm_hostname(void)
{
  return hostname;
}

int comm_gpuid()
{
  //int gpu = rank%getGpuCount();

  return which_gpu;
}

int
comm_rank(void)
{
  return rank;
}

int
comm_size(void)
{
  return size;
}

int
comm_dim(int dir) {

  int i;
  switch(dir) {
  case 0:
    i = xgridsize;
    break;
  case 1:
    i = ygridsize;
    break;
  case 2:
    i = zgridsize;
    break;
  case 3:
    i = tgridsize;
    break;
  default:
    printf("Cannot get direction %d", dir);
    comm_exit(1);
  }

  return i;
}

int
comm_coords(int dir) {

  int i;
  switch(dir) {
  case 0:
    i = xgridid;
    break;
  case 1:
    i = ygridid;
    break;
  case 2:
    i = zgridid;
    break;
  case 3:
    i = tgridid;
    break;
  default:
    printf("Cannot get direction %d", dir);
    comm_exit(1);
  }

  return i;
}

unsigned long
comm_send(void* buf, int len, int dst, void* _request)
{
  
  MPI_Request* request = (MPI_Request*)_request;

  int dstproc;
  int sendtag=99;
  if (dst == BACK_NBR){
    dstproc = back_nbr;
    sendtag = BACK_NBR;
  }else if (dst == FWD_NBR){
    dstproc = fwd_nbr;
    sendtag = FWD_NBR;
  }else{
    printf("ERROR: invalid dest\n");
    comm_exit(1);
  }

  MPI_Isend(buf, len, MPI_BYTE, dstproc, sendtag, MPI_COMM_WORLD, request);  
  return (unsigned long)request;  
}

unsigned long
comm_send_to_rank(void* buf, int len, int dst_rank, void* _request)
{
  
  MPI_Request* request = (MPI_Request*)_request;
  
  if(dst_rank < 0 || dst_rank >= comm_size()){
    printf("ERROR: Invalid dst rank(%d)\n", dst_rank);
    comm_exit(1);
  }
  int sendtag = 99;
  MPI_Isend(buf, len, MPI_BYTE, dst_rank, sendtag, MPI_COMM_WORLD, request);  
  return (unsigned long)request;  
}

int find_neighbor_proc(int which) {
  int proc = -1;
  switch(which){
  case X_BACK_NBR:
    proc = x_back_nbr;
    break;
  case X_FWD_NBR:
    proc = x_fwd_nbr;
    break;
  case Y_BACK_NBR:
    proc = y_back_nbr;
    break;
  case Y_FWD_NBR:
    proc = y_fwd_nbr;
    break;
  case Z_BACK_NBR:
    proc = z_back_nbr;
    break;
  case Z_FWD_NBR:
    proc = z_fwd_nbr;
    break;
  case T_BACK_NBR:
    proc = t_back_nbr;
    break;
  case T_FWD_NBR:
    proc = t_fwd_nbr;
    break;
  default:
    printf("ERROR: invalid dest, line %d, file %s\n", __LINE__, __FILE__);
    comm_exit(1);
  }
  return proc;
}
 

unsigned long
comm_send_with_tag(void* buf, int len, int dst, int tag, void*_request)
{
  MPI_Request* request = (MPI_Request*)_request;
  int dstproc = find_neighbor_proc(dst);
  MPI_Isend(buf, len, MPI_BYTE, dstproc, tag, MPI_COMM_WORLD, request);
  return (unsigned long)request;
}



unsigned long
comm_recv(void* buf, int len, int src, void*_request)
{
  MPI_Request* request = (MPI_Request*)_request;
  
  int srcproc=-1;
  int recvtag=99; //recvtag is opposite to the sendtag
  if (src == BACK_NBR){
    srcproc = back_nbr;
    recvtag = FWD_NBR;
  }else if (src == FWD_NBR){
    srcproc = fwd_nbr;
    recvtag = BACK_NBR;
  }else{
    printf("ERROR: invalid source\n");
    comm_exit(1);
  }
  
  MPI_Irecv(buf, len, MPI_BYTE, srcproc, recvtag, MPI_COMM_WORLD, request);
  
  return (unsigned long)request;
}

unsigned long
comm_recv_from_rank(void* buf, int len, int src_rank, void* _request)
{
  MPI_Request* request = (MPI_Request*)_request;
  
  if(src_rank < 0 || src_rank >= comm_size()){
    printf("ERROR: Invalid src rank(%d)\n", src_rank);
    comm_exit(1);
  }
  
  int recvtag = 99;
  MPI_Irecv(buf, len, MPI_BYTE, src_rank, recvtag, MPI_COMM_WORLD, request);
  
  return (unsigned long)request;
}

unsigned long
comm_recv_with_tag(void* buf, int len, int src, int tag, void* _request)
{ 
  MPI_Request* request = (MPI_Request*)_request;
  int srcproc = find_neighbor_proc(src);
  MPI_Irecv(buf, len, MPI_BYTE, srcproc, tag, MPI_COMM_WORLD, request);
  
  return (unsigned long)request;
}

void* comm_declare_send_relative(void *buffer, int dim, int dir, size_t count)
{
  int back_nbr[4] = {X_BACK_NBR,Y_BACK_NBR,Z_BACK_NBR,T_BACK_NBR};
  int fwd_nbr[4] = {X_FWD_NBR,Y_FWD_NBR,Z_FWD_NBR,T_FWD_NBR};
  int downtags[4] = {XDOWN, YDOWN, ZDOWN, TDOWN};
  int uptags[4] = {XUP, YUP, ZUP, TUP};

  MPI_Request *request = (MPI_Request*)safe_malloc(sizeof(MPI_Request));
  int tag = (dir == 1) ? uptags[dim] : downtags[dim];
  int dst = (dir == 1) ? fwd_nbr[dim] : back_nbr[dim];
  int dstproc = find_neighbor_proc(dst);  
  MPI_Send_init(buffer, count, MPI_BYTE, dstproc, tag, MPI_COMM_WORLD, request);
  return (void*)request;
}

void* comm_declare_receive_relative(void *buffer, int dim, int dir, size_t count)
{
  int back_nbr[4] = {X_BACK_NBR,Y_BACK_NBR,Z_BACK_NBR,T_BACK_NBR};
  int fwd_nbr[4] = {X_FWD_NBR,Y_FWD_NBR,Z_FWD_NBR,T_FWD_NBR};
  int downtags[4] = {XDOWN, YDOWN, ZDOWN, TDOWN};
  int uptags[4] = {XUP, YUP, ZUP, TUP};

  MPI_Request *request = (MPI_Request*)safe_malloc(sizeof(MPI_Request));
  int tag = (dir == 1) ? uptags[dim] : downtags[dim];
  int src = (dir == 1) ? back_nbr[dim] : fwd_nbr[dim];
  int srcproc = find_neighbor_proc(src);  
  MPI_Recv_init(buffer, count, MPI_BYTE, srcproc, tag, MPI_COMM_WORLD, request);
  return (void*)request;
}

void comm_start(void *request)
{
  int rc = MPI_Start( (MPI_Request*)request);
  if (rc != MPI_SUCCESS) {
    printf("ERROR: MPI_Test failed\n");
    comm_exit(1);
  }

  return;
}

int comm_query(void* request) 
{
  MPI_Status status;
  int query;
  int rc = MPI_Test( (MPI_Request*)request, &query, &status);
  if (rc != MPI_SUCCESS) {
    printf("ERROR: MPI_Test failed\n");
    comm_exit(1);
  }

  return query;
}


//this request should be some return value from comm_recv
void 
comm_wait(void* request)
{
  
  MPI_Status status;
  int rc = MPI_Wait( (MPI_Request*)request, &status);
  if (rc != MPI_SUCCESS){
    printf("ERROR: MPI_Wait failed\n");
    comm_exit(1);
  }
  
  return;
}

//we always reduce one double value
void
comm_allreduce(double* data)
{
  double recvbuf;
  int rc = MPI_Allreduce ( data, &recvbuf,1,MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  if (rc != MPI_SUCCESS){
    printf("ERROR: MPI_Allreduce failed\n");
    comm_exit(1);
  }
  
  *data = recvbuf;
  
  return;
} 

void 
comm_allreduce_int(int* data)
{
  int recvbuf;
  int rc = MPI_Allreduce(data, &recvbuf, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  if(rc!=MPI_SUCCESS){
    printf("ERROR: MPI_Allreduce failed\n");
    comm_exit(1); 
  }
  *data = recvbuf;
  return;
}

//reduce n double value
void
comm_allreduce_array(double* data, size_t size)
{
  double recvbuf[size];
  int rc = MPI_Allreduce ( data, &recvbuf,size,MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  if (rc != MPI_SUCCESS){
    printf("ERROR: MPI_Allreduce failed\n");
    comm_exit(1);
  }
  
  memcpy(data, recvbuf, sizeof(recvbuf));
  
  return;
}

//we always reduce one double value
void
comm_allreduce_max(double* data)
{
  double recvbuf;
  int rc = MPI_Allreduce ( data, &recvbuf,1,MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
  if (rc != MPI_SUCCESS){
    printf("ERROR: MPI_Allreduce failed\n");
    comm_exit(1);
  }
  
  *data = recvbuf;
  
  return;
} 

void comm_free(void *handle) {
  host_free((MPI_Request*)handle);
}

// broadcast from rank 0
void
comm_broadcast(void *data, size_t nbytes)
{
  MPI_Bcast(data, (int)nbytes, MPI_BYTE, 0, MPI_COMM_WORLD);
}

void
comm_barrier(void)
{
  MPI_Barrier(MPI_COMM_WORLD);  
}
void 
comm_cleanup()
{
  MPI_Finalize();
}

void
comm_exit(int ret)
{
  MPI_Finalize();
  exit(ret);
}
