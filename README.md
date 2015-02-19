# Execution Engine
Simple execution engine for testing SDP data flow with GPUs

Requires MPI (any).
Requires pthreads
Requires GPUDegrid

*Build*
This now requires code from GPUDegrid. In the build directory invoke
%> git clone https://github.com/SKA-ScienceDataProcessor/GPUDegrid
Then, make sure mpic++ is in your path and invoke 'make'

*Run*
Use mpirun with 3 ranks

%> mpirun -N 3 -n 3 ./EEngine

*Trouble-shooting*

- "no rule to make GPUDegrid/degrid_gpu.cu required by degrid_gpu.o"
   You most likely didnt clone GPUDegrid. See above under "Build"
