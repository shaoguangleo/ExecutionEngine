all: EEngine

server: server.cpp
	mpic++ -o server server.cpp

client: client.cpp
	mpic++ -o client client.cpp

degrid_gpu.o: GPUDegrid/degrid_gpu.cu GPUDegrid/degrid_gpu.cuh GPUDegrid/cucommon.cuh GPUDegrid/Defines.h
	nvcc -c -arch=sm_35 -std=c++11 -o degrid_gpu.o GPUDegrid/degrid_gpu.cu

EEngine: EEngine.cpp GPUDegrid/degrid_gpu.cuh degrid_gpu.o Defines.h 
	mpic++ -std=c++11 -pthread -o EEngine EEngine.cpp degrid_gpu.o 
