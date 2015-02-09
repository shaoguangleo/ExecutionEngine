
#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unistd.h"
#include "limits.h"

/* This test checks to make sure that two MPI_Comm_connects to two different MPI ports
* match their corresponding MPI_Comm_accepts. The root process opens two MPI ports and
* sends the first port to process 1 and the second to process 2. Then the root process
* accepts a connection from the second port followed by the first port.
* Processes 1 and 2 both connect back to the root but process 2 first sleeps for three
* seconds to give process 1 time to attempt to connect to the root. The root should wait
* until process 2 connects before accepting the connection from process 1.
*/
#define rankprintf(format, ...) \
    for (int q=0;q<rank;q++)printf("    "); \
    printf("%d: ",rank); \
    printf(format, ##__VA_ARGS__);

#define MSG_CHECK 0
#define MSG_OK 1
#define MSG_TASK_COMPLETE 2
#define MSG_ACK 3
#define MSG_TASK_COMING 4
#define TASK_STATUS 101
#define TASK_DEGRID 102

void send_task(MPI_Comm comm, int task_id, int token_id, int to) {
  int data[10];
  int rank = 0;
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

void run_server(int rank, int size) {
    char port1[MPI_MAX_PORT_NAME];
    MPI_Comm comm1;
    MPI_Status status;
    int data[10];
    sleep(3);
    //Initialize
    rankprintf("Initialized rank %d\n", rank);
    MPI_Recv(port1, MPI_MAX_PORT_NAME, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
    rankprintf("Rec'd port1: %s\n", port1);
    MPI_Comm_connect(port1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm1);
    rankprintf("Connected port1: %s\n", port1);

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
       if (MSG_TASK_COMING == data[0]) {
          data[0] = MSG_ACK;
          MPI_Send(data, 1, MPI_INT, 0, 0, comm1);
          sleep(1);
          MPI_Recv(data, 3, MPI_INT, 0, 0, comm1, &status);
          rankprintf("Beginning task %d on token %d...\n", data[0], data[1]);
          sleep(1);
          rankprintf("Task %d complete\n", data[0]);
       }
    }
    rankprintf("Closing %s\n", port1);
    MPI_Comm_disconnect(&comm1);
}

void run_client(int rank, int size) {
  int data;
  MPI_Comm comm1, comm2;
  MPI_Status status;
  char port1[MPI_MAX_PORT_NAME];
  char port2[MPI_MAX_PORT_NAME];

  rankprintf("opening ports.\n");fflush(stdout);
  MPI_Open_port(MPI_INFO_NULL, port1);
  MPI_Open_port(MPI_INFO_NULL, port2);
  rankprintf("opened port1: <%s>\n", port1);
  rankprintf("opened port2: <%s>\n", port2);fflush(stdout);

  MPI_Send(port1, MPI_MAX_PORT_NAME, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
  MPI_Send(port2, MPI_MAX_PORT_NAME, MPI_CHAR, 2, 0, MPI_COMM_WORLD);

  rankprintf("accepting port2.\n");fflush(stdout);
  MPI_Comm_accept(port2, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm2);
  rankprintf("accepting port1.\n");fflush(stdout);
  MPI_Comm_accept(port1, MPI_INFO_NULL, 0, MPI_COMM_SELF, &comm1);
  MPI_Close_port(port1);
  MPI_Close_port(port2);

  sleep(2);
  data = MSG_CHECK;
  MPI_Send(&data, 1, MPI_INT, 0, 0, comm2);
  MPI_Recv(&data, 1, MPI_INT, 0, 0, comm2, &status);
  if (MSG_OK != data) rankprintf("Rank 2 not OK\n");

  send_task(comm1, TASK_DEGRID, 45, 1);
  sleep(1);
  send_task(comm2, TASK_DEGRID, 46, 1);
  sleep(1);
  send_task(comm1, TASK_STATUS, 0, 1);

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
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    rankprintf("rank %d of %d\n", rank, size);
    if (size < 3)
    {
        rankprintf("Three processes needed to run this test.\n");fflush(stdout);
        MPI_Finalize();
        return 0;
    }

    if (rank == 0)
    {
       run_client(rank, size);
    }
    else 
    {
       run_server(rank, size);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    MPI_Finalize();
    return 0;
}
