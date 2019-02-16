
Question: Why is the speedup of low intensity runs with iteration-level synchronization the way it is?
	With low intensity runs, the threads are fighting for access to the shared variable. There is alot
	of congestion because the program can calculatulation for the interation is faster than the time it takes
	to aquire a mutex, update the variable, and unlock the mutex

Question: Compare the speedup of iteration-level synchronization to thread-level synchronization. Why is it that way?
	The thread-level speedup is higher than the iteration-level because the access to the shared variable is less congested.
	When the iteration level code runs, the shared varaiable is accessed more frequently, and the threads fight for
	exclusive access to the variable. The reasoning is similar to the answer to the previous question. However, when
	the thread level sync is run, the thread only updates the shared variable after the calculation is finished. The
	shared variable only has `nbthreads' accesses, thus the access is less congested.