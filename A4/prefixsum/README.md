Describe the structure of the parallel algorithm for Prefix Sum. (Highly recommend the “cut in P and fix it” approach.)

The structure of his algo is done in 2 parts. Each thread does prefix sum on a subarray. Then forwards the last result
(largest prefixsum) to the next thread. This value is the offset. Each thread then adds up the previous threads offset
and adds the result to the subarray.

The structure of the parallel algorithm is to divide the array into sub arrays and calculate sum

Run the code on mamba, in the prefixsum/ directory, using make bench. And then plot the
results using make plot. Does the plot make sense? Why?

