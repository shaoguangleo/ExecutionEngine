ifeq ($(PGI),1)
	PTHREAD_FLAG = -L/opt/pgi/linux86-64/14.10/lib/ -lpthread
else
	PTHREAD_FLAG = -pthread
endif

CFLAGS =-std=c++11
CUFLAGS = -arch=sm_35

ifeq ($(DEBUG),1)
	CFLAGS += -g -D__DEBUG
	CUFLAGS += -lineinfo -G
endif


all: EEngine

server: server.cpp
	mpic++ -o server server.cpp

client: client.cpp
	mpic++ -o client client.cpp

degrid_gpu.o: GPUDegrid/degrid_gpu.cu GPUDegrid/degrid_gpu.cuh GPUDegrid/cucommon.cuh GPUDegrid/Defines.h
	nvcc -c ${CUFLAGS} ${CFLAGS} -o degrid_gpu.o GPUDegrid/degrid_gpu.cu

GPUDebug.o: GPUDebug.cu
	nvcc -c ${CUFLAGS} ${CFLAGS} -o GPUDebug.o GPUDebug.cu

EEngine: EEngine.cpp GPUDegrid/degrid_gpu.cuh degrid_gpu.o Defines.h GPUDebug.h GPUDebug.o
	mpic++ ${CFLAGS} -o EEngine EEngine.cpp degrid_gpu.o GPUDebug.o ${PTHREAD_FLAG}
