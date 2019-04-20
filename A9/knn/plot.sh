#!/bin/sh

RESULTDIR=result/
PLOTDIR=plots/


# create result directory
if [ ! -d ${RESULTDIR} ];
then
    mkdir ${RESULTDIR}
fi

# create plot directory
if [ ! -d ${PLOTDIR} ];
then
    mkdir ${PLOTDIR}
fi

# import params
. ../params.sh


# error checking
#   file existence
#   file populate

echo checking file existence.

for N in ${NODES};
do
   for P in ${PS_PER_NODE};
   do 
      NPP=$(expr ${N} \* ${P})
      if [ "${NPP}" -gt "32" ];
      then
         continue
      fi
      if [ ! -s ${RESULTDIR}/knn_mrmpi_${N}_${P} ] ;
      then
         echo ERROR: ${RESULTDIR}/knn_mrmpi_${NP}_${P} not found 
         echo run \'make bench\'  and WAIT for it to complete
         exit 1
      fi
   done
done

echo formatting output.

	     
# format output
# creat time tables
# pretty sure most of this is unnessary
echo "\documentclass{article}" > plots/time_table.tex
echo "\usepackage{listings}" >> plots/time_table.tex
echo "\usepackage{color}" >> plots/time_table.tex
echo "\usepackage{hyperref}" >> plots/time_table.tex
echo "\usepackage{graphics}" >> plots/time_table.tex
echo "\usepackage{graphicx}" >> plots/time_table.tex
echo "\begin{document}" >> plots/time_table.tex


   # build column headers
   COL_ARGS="{ | l |"
   HEADER=" Cores per Node "
   for N in ${NODES};
   do
      COL_ARGS="${COL_ARGS} l |"
      HEADER="${HEADER} & ${N}"
   done 

   COL_ARGS="${COL_ARGS} }"
   HEADER="${HEADER} \\\\"
   echo "\section{Word Frequency Execution Time}" >> plots/time_table.tex
   echo "\begin{tabular} ${COL_ARGS} " >> plots/time_table.tex
   echo "\hline" >> plots/time_table.tex
   echo " & \multicolumn{4}{ c| }{Nodes} \\\\ \cline{2-5}" >> plots/time_table.tex
   echo  ${HEADER} >> plots/time_table.tex
   echo "\hline" >> plots/time_table.tex

   for P in ${PS_PER_NODE};
   do
      ROW="${P} "
      for N in ${NODES};
      do

         ROW="${ROW} & $(cat ${RESULTDIR}/knn_mrmpi_${N}_${P})"
#     echo ${N} ${NP} $(cat ${RESULTDIR}/mpi_matmul_${N}_${ITER}_${NP})
      done
      echo ${ROW} "\\\\ \hline" >> plots/time_table.tex
   done

   echo "\hline" >> plots/time_table.tex
      echo "\end{tabular}" >> plots/time_table.tex

echo "\end{document}" >> plots/time_table.tex

echo building pdf

cd plots
pdflatex -interaction=nonstopmode time_table.tex  > /dev/null


echo cleanup

# clean up from pdflatex
EXTS="aux log out tex"
for EXT in ${EXTS} ;
do
  rm time_table.${EXT}
done

