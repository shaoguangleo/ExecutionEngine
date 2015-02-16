#! /bin/sh
# Source this script to load appropriate
# modules for buidling EEngine on the PSG
# cluster

module purge

module load cuda/6.5
#module load pgi/14.10
module load gcc/4.8.2
module load numactl/2.0.9
module load hwloc/1.5.1
module load mvapich2-2.0/gcc-4.8.2/cuda-6.5

export MV2_SUPPORT_DPM=1
export MV2_USE_OSU_NB_COLLECTIVES=0 #WAR for bug in mvapich2-2.0. Fixed in 2.1
       # http://mailman.cse.ohio-state.edu/pipermail/mvapich-discuss/2014-November/005255.html
