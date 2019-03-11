
Describe the structure of the parallel algorithm for Merge Sort that does not try to make Merge
parallel. How would you map that parallelism to OpenMP’s looping construct?

The structure of the parallel mergesort is very similar to the serial mergesort. The alogrithm groups
the elements by 2s (next to each other) and sorts the subarray. Then the algo loops over the array again
and groups the elements by 4. The first 2 and next 2 are sorted from the previous iteration. After these
2 subarrays are merges it continues grouping the arrays by 8, then 16, until the array is sorted.
Only the inside loop can be parallelized because you must finish the 2's merge before you can start the
groups of 4 merger (there are ways around this, but utilizes advanced thread synchronization).



Run the code on mamba, in the mergesort/ directory, using make bench. And then plot the
results using make plot. Does the plot make sense? Why?

The plots do make sense because as the number of threads increase the speedup increases.
As the length of the data decrease, the more the threads, the less speed up there is because
of congestion.