Question: Compare performance at 16 threads across the different synchronization modes. Why are the speedup this way?
	The chunk level sync performs really bad at lower granularity as the code has to sync with the shared variable more
	frequently. Other than that, both chunk and thread sync methods compare the same at higher granularity.


Question: For thread-level synchronization, compare the performance at 16 threads of different n and intensity. Why are the plots this way?
	When n = 1 and intensity = 1, there is no speed up. This is because with n = 1, there is no possible way to parallel this. One thread
	will take the one batch of n values and the other threads will just end. This is very similar to when n = 100. There just isn't enough
	processing needed to effectively multithread the program. At n = 10,000 there is enough work that we need to do that the effect of
	parallelism is starting to show. However, the speedup is minimal too. At granularity of 1, the code resembles the iteration code and access
	to the shared variable is excessive, thus the very little speed up. At the opposite spectrum, at granularity of 1e+06, the code resembles a
	single thread of execution because one thread takes all the work and the other threads get starved. This is why the graph looks like a
	bell curve. This is the same for n = 1,000,000. For n = 100'000'000, the parallelism is more apparent. The graph takes more of a sqrt(x)
	graph. This is because the granularity of 1e+06 is a good granularity that gives the thread enough work to do so it doesn't fight for the
	shared variable access, but not too much work that the dynamic scheduling is not apparent. As you increase the intensity of the run,
	the more speedup is more apparent (from what I understand) because the function call takes a lot longer.