#include <mpi.h>
#include <math.h>
#include <iostream>

#include <chrono>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif
  double generate2DHeat(long n, long global_i, long global_j);

  int check2DHeat(long n, long global_i, long global_j, double v, long k); //this function return 1 on correct. 0 on incorrect. Note that it may return 1 on incorrect. But a return of 0 means it is definitely incorrect
#ifdef __cplusplus
}
#endif


size_t get_index(int n, int i, int j) {
  if      (i >= n) i = n - 1;
  else if (i < 0)  i = 0;

  if      (j >= n) j = n - 1;
  else if (j < 0)  j = 0;

  return (i * n) + j;
}


int get_i(int n, size_t i) {
  return i / n;
}


int get_j(int n, size_t i) {
  return i % n;
}


double get_value(double* last, int iter, int n, int i, int j) {
  if (iter == 0) { // if initial iteration
    return generate2DHeat(n, i, j);
  } else {
    double val{ };

    val += last[get_index(n, i - 1, j    )];
    val += last[get_index(n, i    , j - 1)];
    val += last[get_index(n, i    , j    )];
    val += last[get_index(n, i    , j + 1)];
    val += last[get_index(n, i + 1, j    )];

    val *= (1.0 / 5.0);

    return val;
  }
}


void do_parent_work(long N, long K, int size) {
  // Send work to each node (which iteration)
  const int data_size = N * N;
  std::unique_ptr<double[]> data{ new double[data_size] };
  std::unique_ptr<double[]> prev_data{ new double[data_size] };

  for (int iteration = 0; iteration < K; ++iteration) {
    // send initial data
    for (int node = 1; node < size; ++node) {
      MPI_Send(&iteration, 1, MPI_INT, node, 0, MPI_COMM_WORLD);
      MPI_Send(prev_data.get(), data_size, MPI_DOUBLE, node, 0, MPI_COMM_WORLD);
    }

    // recive data
    for (int node = 1; node < size; ++node) {
      // get the buffer the node stored its work on
      std::unique_ptr<double[]> recv_data{ new double[data_size] };
      MPI_Recv(recv_data.get(), data_size, MPI_DOUBLE, node, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      // get the bounds of work the node did
      int start, end;
      MPI_Recv(&start, 1, MPI_INT, node, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      MPI_Recv(&end, 1, MPI_INT, node, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      // copy what we got from the node into our master buffer
      std::copy(recv_data.get() + start, recv_data.get() + end, data.get() + start);
    }

    // check the data??

    // swap the old data to prepare for new iteration
    std::swap(data, prev_data);
  }

  // send stop bits
  int iteration = -1;
  for (int node = 1; node < size; ++node) {
    MPI_Send(&iteration, 1, MPI_INT, node, 0, MPI_COMM_WORLD);
  }

  for (int i = 0; i < data_size; ++i) {
    int r = check2DHeat(N, get_i(N, i), get_j(N, i), data[i], K);
    if (r == 0) std::cout << "W" << std::endl;
  }
}


void do_child_work(long N, int rank, int size) {
  const int data_size = N * N;
  const int gran = data_size / size;
  const int start = rank * gran;
  const int end = (rank == size - 1)? N : (rank + 1) * gran;

  while (true) {
    // recive iteration number
    int iteration;
    MPI_Recv(&iteration, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    if (iteration == -1) return; // stop bit

    // recive previous data
    std::unique_ptr<double[]> prev_data{ new double[data_size] };
    std::unique_ptr<double[]> data{ new double[data_size] };
    MPI_Recv(prev_data.get(), data_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // do calculations
    for (int idx = start; idx < end; ++idx) {
      data[idx] = get_value(prev_data.get(), iteration, N, get_i(N, idx), get_j(N, idx));
    }

    // send back data
    MPI_Send(data.get(), data_size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    MPI_Send(&start, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    MPI_Send(&end, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
  }
}

void do_all_work(int N, int K) {
  const int data_size = N * N;
  std::unique_ptr<double[]> data{ new double[data_size] };
  std::unique_ptr<double[]> prev_data{ new double[data_size] };

  for (int iteration = 0; iteration <= K; ++iteration) {
    for (int idx = 0; idx < data_size; ++idx) {
      data[idx] = get_value(prev_data.get(), iteration, N, get_i(N, idx), get_j(N, idx));
    }

    std::swap(data, prev_data);
  }

  for (int i = 0; i < data_size; ++i) {
    int r = check2DHeat(N, get_i(N, i), get_j(N, i), prev_data[i], K);
    if (r == 0) std::cout << "W: " << get_j(N, i) << "  " << get_i(N, i) << std::endl;
  }
}


int main(int argc, char* argv[]) {

  if (argc < 3) {
    std::cerr<<"usage: mpirun "<<argv[0]<<" <N> <K>"<<std::endl;
    return -1;
  }

  // declare and init command line params
  long N, K;
  N = atol(argv[1]);
  K = atol(argv[2]);

  auto start = std::chrono::steady_clock::now();

  MPI_Init(&argc, &argv);

  int rank{  }, size{  };
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size == 1) {
    do_all_work(N, K);

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapse = end - start;
    std::cerr << elapse.count() << std::endl;
  } else if (rank == 0) {
    do_parent_work(N, K, size);

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapse = end - start;
    std::cerr << elapse.count() << std::endl;
  } else {
    // When people get touchy using master/slave
    do_child_work(N, rank, size);
  }

  MPI_Finalize();

  return 0;
}

