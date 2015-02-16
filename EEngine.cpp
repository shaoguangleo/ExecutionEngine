//
//  Author: 
//  
//    Levi Barnes
//
//  Copyright 2015 

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unistd.h"
#include "limits.h"
#include <pthread.h>
#include "time.h"
#include "math.h"  //exp
#include "GPUDegrid/degrid_gpu.cuh"
#include "Defines.h"

typedef struct {float x,y;} float2;
typedef struct {double x,y;} double2;

int rank;
FILE* logfid;

#define rankprintf(format, ...) \
    { time_t t = time(NULL); \
    char* foo = asctime(localtime(&t));\
    foo[24]='\0';\
    fprintf(logfid, "(%s)  ", foo); \
    fprintf(logfid, format, ##__VA_ARGS__); \
    for (int q=0;q<rank;q++) printf("    ");\
    printf("%d: ", rank);\
    printf(format, ##__VA_ARGS__); \
    fflush(logfid); }

//Length of each task message
//The first item is the task type
//If you create a task that needs more than TASK_LEN-1 integers, increase TASK_LEN
#define TASK_LEN 3

//Messages
#define MSG_CHECK 0
#define MSG_OK 1
#define MSG_STATUS 2
#define MSG_TASK_COMPLETE 3
#define MSG_ACK 4
#define MSG_TASK_COMING 5

//Task types
#define TASK_STATUS 101
#define TASK_DEGRID 102
#define TASK_SENDTOK 103
#define TASK_RECVTOK 104
#define TASK_KILLTOK 105
#define TASK_GENVIS 106
#define TASK_GENIMG 107

//Status responses
#define STATUS_IDLE 0
#define STATUS_BUSY 1

//Token types
#define TOKEN_VIS 1
#define TOKEN_IMG 2


typedef struct {
   int tid;
   int token_type; //TOKEN_VIS or TOKEN_IMG
   size_t size;
   void* data;
   int on_rank;
} token;

void throw_error(const char* txt) {
   fprintf(stderr, "ERROR: %s\n", txt);
}

//TODO make variadic
//template <integer... MSG>
//void send_task(MPI_Comm comm, MSG... in) {
void send_task(MPI_Comm comm, int task_id, int token_id, int to) {
  int data[10];
  //int msg_data[]={in...};
  data[0] = MSG_TASK_COMING;
  MPI_Status status;
  MPI_Send(data, 1, MPI_INT, 0, 0, comm);
  MPI_Recv(data, 1, MPI_INT, 0, 0, comm, &status);
  if (MSG_ACK != data[0]) {
     MPI_Send(data, 1, MPI_INT, 0, 0, comm);
     MPI_Recv(data, 1, MPI_INT, 0, 0, comm, &status);
  }
  if (MSG_ACK != data[0]) {
     rankprintf("Rank 1 not OK\n");
  } else {
     data[0] = task_id;
     data[1] = token_id;
     data[2] = to;
     rankprintf("Sending task %d, %d, %d to node %d\n", task_id, token_id, to, to);
     MPI_Send(data, TASK_LEN, MPI_INT, 0, 0, comm);
  }

}

pthread_t tid[2];
pthread_mutex_t lock; 
int node_status=45;
void* gpu_mem;
token* token_list[256];

int PAD(const int in) {
   int pad_size = 128/sizeof(PRECISION2);
   int padded_npoints = ((in + pad_size - 1) / pad_size) * pad_size;
}

template <class CmplxType>
void init_gcf(CmplxType *gcf, size_t size) {

  for (size_t sub_x=0; sub_x<8; sub_x++ )
   for (size_t sub_y=0; sub_y<8; sub_y++ )
    for(size_t x=0; x<size; x++)
     for(size_t y=0; y<size; y++) {
       //Some nonsense GCF
       auto tmp = gcf[0].x; //just to get the type
       tmp = sin(6.28*x/size/8)*exp(-(1.0*x*x+1.0*y*y*sub_y)/size/size/2);
       gcf[size*size*(sub_x+sub_y*8)+x+y*size].x = tmp*sin(1.0*x*sub_x/(y+1));
       gcf[size*size*(sub_x+sub_y*8)+x+y*size].y = tmp*cos(1.0*x*sub_x/(y+1));
       //std::cout << tmp << gcf[x+y*size].x << gcf[x+y*size].y << std::endl;
     }

}
void* exec_task(void* data_in) {
   int* data = (int*) data_in;
   rankprintf("Beginning task %d on token %d. Setting status to BUSY.\n", data[0], data[1]);

   //Change status to IDLE
   pthread_mutex_lock(&lock);
   node_status = STATUS_BUSY;
   pthread_mutex_unlock(&lock);

   if (TASK_GENVIS == data[0]) {
      //Generate some visibilities
      //This won't be needed long term
      // data[0] : task type
      // data[1] : tokenid;
      // data[2] : size
      int next_tok=0;
      while(token_list[next_tok] != 0 && next_tok < 256) next_tok++;
      if(next_tok>=256) throw_error("Too many tokens\n");
      token_list[next_tok] = new token;
      token_list[next_tok]->tid = data[1];
      token_list[next_tok]->token_type = TOKEN_VIS;
      int npts = data[2];
      token_list[next_tok]->size = npts;
      //TODO padding for degrid
      token_list[next_tok]->data = malloc(sizeof(PRECISION2)*2*PAD(NPOINTS)); //in and out
      token_list[next_tok]->on_rank = rank;
      //Random visibilities
      for(size_t n=0; n<npts;n++) {
         ((PRECISION2*)data)[n].x = ((float)rand())/RAND_MAX*1000;
         ((PRECISION2*)data)[n].y = ((float)rand())/RAND_MAX*1000;
      }
      sleep(4); //TODO remove
   } else if (TASK_GENIMG == data[0]) {
      //Generate an image
      //This won't be needed long term
      // data[0] : task type
      // data[1] : tokenid
      // data[2] : size
      int next_tok=0;
      while(token_list[next_tok] != 0 && next_tok < 256) next_tok++;
      if(next_tok>=256) throw_error("Too many tokens\n");
      token_list[next_tok] = new token;
      token_list[next_tok]->tid = data[1];
      token_list[next_tok]->token_type = TOKEN_VIS;
      int img_dim = data[2];
      token_list[next_tok]->size = img_dim;
      //TODO padding for degrid
      token_list[next_tok]->data = malloc(sizeof(PRECISION2)*img_dim*img_dim); //out and in 
      token_list[next_tok]->on_rank = rank;
      //Random visibilities
      for(size_t x=0; x<img_dim;x++)
      for(size_t y=0; y<img_dim;y++) {
         ((PRECISION2*)data)[x+img_dim*y].x = exp(-((x-1400.0)*(x-1400.0)+(y-3800.0)*(y-3800.0))/8000000.0)+1.0;
         ((PRECISION2*)data)[x+img_dim*y].y = 0.4;
      }
      sleep(4); //TODO remove
      
   } else if (TASK_KILLTOK == data[0]) {
      free(token_list[data[0]]->data);
      delete token_list[data[0]];
      token_list[data[0]] = NULL;
   } else if (TASK_DEGRID == data[0]) {
      // Degrid
      // data[1] : visibility tokenid
      // data[2] : image tokenid 
      int vistok=0, imgtok=0;
      while(token_list[vistok]->tid == data[1]) vistok++;
      while(token_list[imgtok]->tid == data[2]) imgtok++;
      
      int img_size = token_list[imgtok]->size;
      PRECISION2* img = (PRECISION2*) token_list[imgtok]->data;
      int npts = token_list[vistok]->size;
      PRECISION2* out = (PRECISION2*) token_list[vistok]->data;
      PRECISION2* in = out + npts;

      PRECISION2* gcf = (PRECISION2*)malloc(sizeof(PRECISION2*)*GCF_DIM*GCF_DIM);
      init_gcf(gcf, GCF_DIM);
      degridGPU(out, in, npts, img, img_size, gcf, GCF_DIM);
   } else {
      sleep(2);
   }

   rankprintf("Task complete. Setting status to IDLE.\n");
   //Change status to IDLE
   pthread_mutex_lock(&lock);
   node_status = STATUS_IDLE;
   pthread_mutex_unlock(&lock);
  
   return NULL;
}

void run_server(int size) {
    char port1[MPI_MAX_PORT_NAME];
    MPI_Comm comm1;
    MPI_Status status;
    int data[TASK_LEN];
    bool waiting;
    memset(token_list, 0, sizeof(token*)*256);
    sleep(3);
    //Initialize
    //rankprintf("Initialized rank %d\n", rank);
    MPI_Recv(port1, MPI_MAX_PORT_NAME, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
    //rankprintf("Rec'd port1: %s\n", port1);
    MPI_Comm_connect(port1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm1);
    //rankprintf("Connected port1: %s\n", port1);
    pthread_mutex_init(&lock, NULL);

    //Receive tasks
    data[0] = 0;
    while(data[0] != INT_MAX) {
       data[0] = 0;
       MPI_Recv(data, 1, MPI_INT, 0, 0, comm1, &status);
       rankprintf("Received %d from root\n", data[0]);
       if (MSG_CHECK == data[0]) {
          data[0] = MSG_OK;
          MPI_Send(data, 1, MPI_INT, 0, 0, comm1);
       }
       if (MSG_STATUS == data[0]) {
          pthread_mutex_lock(&lock);
          data[0] = node_status;
          rankprintf("status is %d\n", node_status);
          if (waiting && STATUS_IDLE == node_status) {
             pthread_join(tid[0], NULL);
             waiting = false;
          }
          MPI_Send(data, 1, MPI_INT, 0, 0, comm1);
          pthread_mutex_unlock(&lock);
       }
       if (MSG_TASK_COMING == data[0]) {
          data[0] = MSG_ACK;
          MPI_Send(data, 1, MPI_INT, 0, 0, comm1);
          MPI_Recv(data, TASK_LEN, MPI_INT, 0, 0, comm1, &status);
          int err = pthread_create(&(tid[0]), NULL, &exec_task, (void*)data);
          if (err) printf("ERROR: Could not open pthread\n");
          waiting = true;
          sleep(1);
       }
    }
    rankprintf("Closing %s\n", port1);
    MPI_Comm_disconnect(&comm1);
    pthread_mutex_destroy(&lock);
}

int get_status(MPI_Comm comm1) {
  int data = MSG_STATUS;
  MPI_Status status;
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm1);
  MPI_Recv(&data, 1, MPI_INT, 0, 0, comm1, &status);
  return data;
}

class task {
   public:
   int task_msg[TASK_LEN];
   bool sent;
   bool complete;
   //TODO make variadic
   task(int in1, int in2, int in3) {
      task_msg[0] = in1;
      task_msg[1] = in2;
      task_msg[2] = in3;
      sent = false;
      complete = false;
   }
   void display() {
      rankprintf("task: %d, %d, %d\n", task_msg[0], task_msg[1], task_msg[2]);
      if (sent) { 
         rankprintf("Sent.\n");
      } else {
         rankprintf("Not sent.\n");
      }
      if (complete) { 
         rankprintf("Complete.\n");
      } else {
         rankprintf("Not complete.\n");
      }
   }
};

class taskQueue {
   task* task_list;
   MPI_Comm this_comm;
   int n_tasks;
   int last_sent;
   public:
   taskQueue(MPI_Comm comm_in) {
      this_comm = comm_in;
      task_list = (task*)malloc(sizeof(task)*500);
      n_tasks = 0;
      last_sent = -1;
   }
   ~taskQueue() {
      free(task_list);
   }
   int append(task t_in) {
      t_in.sent = false;
      t_in.complete = false;
      task_list[n_tasks] = t_in;
      n_tasks++;
      return n_tasks-1;
   }
   int send_next() {
      int q=0;
      while(true == task_list[q].sent && q < n_tasks) { 
         q++;
      }
      if (500 == q) {
         throw_error("No task");
         return -1;
      }
      send_task(this_comm, task_list[q].task_msg[0],
                           task_list[q].task_msg[1],
                           task_list[q].task_msg[2]);
      task_list[q].sent = true;
      last_sent = q;
      return q; 
   }
   bool done() {
      int q=0;
      //rankprintf("last_snt = %d\n", last_sent);
      if (-1 == last_sent) return true;
      //task_list[last_sent].display();
      if (true != task_list[last_sent].sent) throw_error("Task never sent");
      if (true == task_list[last_sent].complete) return true;
      else {
         int data = get_status(this_comm);
         rankprintf("Rec'd status %d from node 1\n", data);
         if (STATUS_IDLE == data) {
            task_list[last_sent].complete = true;
            return true;
         }
         else return false;
      }
   }
   bool empty() {
      if (0 == n_tasks) return true;
      return task_list[n_tasks-1].sent;
   }
};

void run_client(int size) {
  int data;
  MPI_Comm comm1, comm2;
  MPI_Status status;
  char port1[MPI_MAX_PORT_NAME];
  char port2[MPI_MAX_PORT_NAME];

  //rankprintf("opening ports.\n");fflush(stdout);
  MPI_Open_port(MPI_INFO_NULL, port1);
  MPI_Open_port(MPI_INFO_NULL, port2);
  //rankprintf("opened port1: <%s>\n", port1);
  //rankprintf("opened port2: <%s>\n", port2);fflush(stdout);

  MPI_Send(port1, MPI_MAX_PORT_NAME, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
  MPI_Send(port2, MPI_MAX_PORT_NAME, MPI_CHAR, 2, 0, MPI_COMM_WORLD);

  //rankprintf("accepting port2.\n");fflush(stdout);
  MPI_Comm_accept(port2, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm2);
  //rankprintf("accepting port1.\n");fflush(stdout);
  MPI_Comm_accept(port1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm1);
  MPI_Close_port(port1);
  MPI_Close_port(port2);

  sleep(2);
  //Ping both nodes
  data = MSG_CHECK;
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm1);
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm2);
  sleep(1); 
  MPI_Recv(&data, 1, MPI_INT, 0, 0, comm1, &status);
  if (MSG_OK != data) {
     sleep(3);
     MPI_Recv(&data, 1, MPI_INT, 0, 0, comm1, &status);
  }
  if (MSG_OK != data) {
    rankprintf("Rank 1 not OK\n");
  } else {
    rankprintf("Rank 1 OK\n");
  } 
  MPI_Recv(&data, 1, MPI_INT, 0, 0, comm2, &status);
  if (MSG_OK != data) {
     sleep(3);
     MPI_Recv(&data, 1, MPI_INT, 0, 0, comm2, &status);
  }
  if (MSG_OK != data) {
    rankprintf("Rank 2 not OK\n");
  } else {
    rankprintf("Rank 2 OK\n");
  } 

  //Create and fill two queues
  taskQueue q1(comm1);
  taskQueue q2(comm2);
  q1.append(task(TASK_GENVIS, 45, 1));
  q1.append(task(TASK_GENIMG, 46, 1));
  //q1.append(task(TASK_DEGRID, 45, 46));
  q2.append(task(TASK_GENVIS, 47, 2));
  q2.append(task(TASK_GENIMG, 48, 2));
  //q2.append(task(TASK_DEGRID, 47, 48));

  //Submit tasks until queues are empty
  while (!(q1.empty() && q2.empty())) {
     if (q1.done() && !q1.empty()) q1.send_next();
     if (q2.done() && !q2.empty()) q2.send_next();
     sleep(1);
  }
  //Wait for final tasks to complete
  while (!(q1.done() && q2.done())) {
     sleep(1);
  }

  rankprintf("Closing\n");
  //Close connections
  data = INT_MAX;
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm2);
  sleep(2);
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm1);

  MPI_Comm_disconnect(&comm1);
  MPI_Comm_disconnect(&comm2);

}


int main( int argc, char *argv[] )
{
    int size;
    char filename[100];

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    sprintf(filename, "log%d.txt", rank);
    logfid = fopen(filename,"a");
    rankprintf("rank %d of %d\n", rank, size);
    if (size < 3)
    {
        rankprintf("Three processes needed to run this test.\n");fflush(stdout);
        MPI_Finalize();
        return 0;
    }

    if (rank == 0)
    {
       run_client(size);
    }
    else 
    {
       run_server(size);
    }
    fclose(logfid);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
