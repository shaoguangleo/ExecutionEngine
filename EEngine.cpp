
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unistd.h"
#include "limits.h"
#include <pthread.h>
#include "time.h"

/* This test checks to make sure that two MPI_Comm_connects to two different MPI ports
* match their corresponding MPI_Comm_accepts. The root process opens two MPI ports and
* sends the first port to process 1 and the second to process 2. Then the root process
* accepts a connection from the second port followed by the first port.
* Processes 1 and 2 both connect back to the root but process 2 first sleeps for three
* seconds to give process 1 time to attempt to connect to the root. The root should wait
* until process 2 connects before accepting the connection from process 1.
*/
#define rankprintf(format, ...) \
    { time_t t = time(NULL); \
    fprintf(fid, "(%s)  ", asctime(localtime(&t))); \
    fprintf(fid, format, ##__VA_ARGS__); \
    for (int q=0;q<rank;q++) printf("    ");\
    printf("%d: ", rank);\
    printf(format, ##__VA_ARGS__); }

#define MSG_CHECK 0
#define MSG_OK 1
#define MSG_STATUS 2
#define MSG_TASK_COMPLETE 3
#define MSG_ACK 4
#define MSG_TASK_COMING 5
#define TASK_STATUS 101
#define TASK_DEGRID 102

#define STATUS_IDLE 0
#define STATUS_BUSY 1

int rank;
FILE* fid;

void send_task(MPI_Comm comm, int task_id, int token_id, int to) {
  int data[10];
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
     MPI_Send(data, 3, MPI_INT, 0, 0, comm);
  }

}
pthread_t tid[2];
pthread_mutex_t lock; 
int node_status=45;
void* exec_task(void* data_in) {
   int* data = (int*) data_in;
   rankprintf("Beginning task %d on token %d. Setting status to BUSY.\n", data[0], data[1]);

   //Change status to IDLE
   pthread_mutex_lock(&lock);
   node_status = STATUS_BUSY;
   pthread_mutex_unlock(&lock);

   sleep(5);

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
    int data[10];
    bool waiting;
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
          MPI_Recv(data, 3, MPI_INT, 0, 0, comm1, &status);
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

  //Send a task to node 1 and 2
  send_task(comm1, TASK_DEGRID, 45, 1);
  sleep(1);
  rankprintf("Rec'd status %d from node 1\n", get_status(comm1))
  rankprintf("Rec'd status %d from node 2\n", get_status(comm2));
  send_task(comm2, TASK_DEGRID, 46, 1);
  rankprintf("Rec'd status %d from node 2\n", get_status(comm2));
  sleep(1);
  //Wait for node 1 to finish
  while (data != STATUS_IDLE) {
     data = get_status(comm1);
     rankprintf("Rec'd status %d from node 1\n", data);
     sleep(1);
  }
  //Wait for node 2 to finish
  while (data != STATUS_IDLE) {
     data = get_status(comm2);
     rankprintf("Rec'd status %d from node 2\n", data);
     sleep(1);
  }
  //Send a new task to node 1 and wait
  send_task(comm1, TASK_STATUS, 0, 1);
  while (data != STATUS_IDLE) {
     data = get_status(comm1);
     rankprintf("Rec'd status %d from node 1\n", data);
     sleep(1);
  }

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
    fid = fopen(filename,"a");
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
    fclose(fid);
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
