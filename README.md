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
Set MV2_SUPPORT_DPM=1 in your run script
Use mpirun with 3 ranks

%> mpirun -N 3 -n 3 ./EEngine

*Trouble-shooting*

- "no rule to make GPUDegrid/degrid_gpu.cu required by degrid_gpu.o"
   You most likely didnt clone GPUDegrid. See above under "Build"
- MPID_Open_Port no implemented
   Make sure to set MV2_SUPPORT_DPM=1 on each node

*Cluster specific utilities*
NVIDIA PSG cluster:
    source setup_psg.sh to load appropriate modules, set environment variables
    qsub sub.sh to submit a job on 3 nodes

Wilkes:
    source setup_wilkes.sh
    sbatch wilkes_submit.tesla

