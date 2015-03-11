#! /bin/sh

#PBS -l nodes=1,walltime=00:10:00
cd /home-2/lbarnes/SKA/clean/ExecutionEngine/
source ./setup_psg.sh
export TMPDIR=/home-2/lbarnes/tmp
#mpirun -n 3 -ppn 3 ./EEngine
mpirun -n 3 -ppn 3 nvprof -o EE.%q{MV2_COMM_WORLD_RANK}.prof ./EEngine
#mpirun -n 3 -ppn 3 ./a.out

