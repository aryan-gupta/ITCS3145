#sleep 20

. ../params.sh

echo This scripts queues the job one at a time and needs to wait for the next job to be completed. This should only take a couple of minutes.

for N in ${NODES};
do
   for P in ${PS_PER_NODE};
   do 
      NPP=$(expr ${N} \* ${P})
      if [ "${NPP}" -le "32" ];
      then
         qsub -q mamba -l walltime=00:10:00 -d "$(pwd)" -l nodes=${N}:ppn=${P} ./run-knn.sh
      fi

      
      # ensure one job running at a time
      # This is necessary to avoid bottlenecking the IO system (Erik on 04/14/2019)
      while [[ -n "$(qstat -u ${USER} -i)" || -n "$(qstat -u ${USER} -r)" ]];
      do
        sleep 3 
      done 

   done
done

#qsub -q mamba -l walltime=02:00:00 -d $(pwd) -l nodes=4:ppn=6 ./run.sh
#qsub -q mamba -l walltime=02:00:00 -d $(pwd) -l nodes=5:ppn=5 ./run.sh
#qsub -q mamba -l walltime=02:00:00 -d $(pwd) -l nodes=4:ppn=3 ./run.sh
#qsub -q mamba -l walltime=02:00:00 -d $(pwd) -l nodes=2:ppn=8 ./run.sh
#qsub -q mamba -l walltime=02:00:00 -d $(pwd) -l nodes=1:ppn=1 ./run.sh

