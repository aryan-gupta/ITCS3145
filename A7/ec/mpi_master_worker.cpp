#include <mpi.h>
#include <iostream>

#include <iostream>
#include <chrono>
#include <array>
#include <memory>
#include <vector>

using func_t = float (*)(float, int);

#ifdef __cplusplus
extern "C" {
#endif

float f1(float x, int intensity);
float f2(float x, int intensity);
float f3(float x, int intensity);
float f4(float x, int intensity);

#ifdef __cplusplus
}
#endif

constexpr size_t DATA_SIZE = 7;

float integrate(func_t functionid, int a, int b, int n, int s, int e, int intensity) {
  float ban = (b - a) / (float)n;
  float ans{  };

  for (int i = s; i < e; ++i) {
    float x = a + ((float)i + 0.5) * ban;
    ans += functionid(x, intensity);
  }

  return ans;
}

float do_parent_work(int size, int fid, int a, int b, int n, int intensity) {
  int gran = n / size - 1;
  float ans{  };

  std::array<int, DATA_SIZE> data = { fid, a, b, n, 0, gran, intensity };

  for (int i = 1; i < size - 1; ++i) {
    MPI_Send(data.data(), DATA_SIZE, MPI_INT, i, 0, MPI_COMM_WORLD);
    data[4] = data[5];
    data[5] += gran;
  }

  data[5] = n;
  MPI_Send(data.data(), DATA_SIZE, MPI_INT, size - 1, 0, MPI_COMM_WORLD);

  data[0] = 0;
  for (int i = 1; i < size; ++i) {
    MPI_Send(data.data(), DATA_SIZE, MPI_INT, i, 0, MPI_COMM_WORLD);
  }

  for (int i = 1; i < size; ++i) {
    float partial;
    MPI_Recv(&partial, 1, MPI_FLOAT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    ans += partial;
  }

  ans *= (b - a) / (float)n;

  return ans;
}


// http://supercomputingblog.com/mpi/mpi-tutorial-5-asynchronous-communication/
// The basic premise is this:
// The child gets one chunk of data. The data recv must be blocking
// the child then sets up an async recv for the next chunk then starts the calculations
// While MPI gets the next set of data, we do our calculations
// Once we have our answer, we wait for any previous async send.
// NOTE: On the first iteration, the request handle is empty. So it will fail. According to
//       docs, this is communicated by the status, however we just ignore it.
// NOTE: Turns out I was wrong on the previous assumption, need a bool not to tell us if
//       if our data is valid or not.
// Once the previous async send is finished, we send our data.
// Then we wait on the recv we started before the calculations. Once we have the data
// we just do a pointer swap of data and async_data so we dont have any race conditions
// If our async recv is the last chunk, then we just cancel our recv request
void do_child_work() {
  std::unique_ptr<int[]> data{ new int[DATA_SIZE] };
  std::unique_ptr<int[]> async_data{ new int[DATA_SIZE] };
  float ans{  };
  MPI_Request send;
  bool wait_for_send = false;

  // recive the initial chunk, this must be a blocking recv
  MPI_Recv(data.get(), DATA_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  while (true) {
    MPI_Request request; // before we start any computations, go head and setup the async recv
    MPI_Irecv(async_data.get(), DATA_SIZE, MPI_INT, 0, 0, MPI_COMM_WORLD, &request);

    int id = data[0];
    int a  = data[1];
    int b  = data[2];
    int n  = data[3];
    int s  = data[4];
    int e  = data[5];
    int i  = data[6];

    // depending on the function id. Set the function pointer to the pointer to the function
    func_t func = nullptr;
    switch (id) {
      case 0: {
        MPI_Cancel(&request);
        return; // if our function id is 0 then there is no more work to do
      }
      case 1: func = f1; break;
      case 2: func = f2; break;
      case 3: func = f3; break;
      case 4: func = f4; break;
      default: {
        std::cout << "[E] Ya, you screwed up... have fun" << std::endl;
        std::terminate();
      }
    }

    float tmp_ans = integrate(func, a, b, n, s, e, i);

    if (wait_for_send) MPI_Wait(&send, MPI_STATUS_IGNORE); // wait for any previous send
    ans = tmp_ans;
    MPI_Isend(&ans, 1, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, &send);
    wait_for_send = true;

    // wait on async recive and swap our data for the next iteration
    MPI_Wait(&request, MPI_STATUS_IGNORE);
    std::swap(data, async_data);
  }
}



int main (int argc, char* argv[]) {

  if (argc < 6) {
    std::cerr<<"usage: mpirun "<<argv[0]<<" <functionid> <a> <b> <n> <intensity>"<<std::endl;
    return -1;
  }

  int id = std::atoi(argv[1]);
  int a  = std::atoi(argv[2]);
  int b  = std::atoi(argv[3]);
  int n  = std::atoi(argv[4]);
  int i  = std::atoi(argv[5]);

  auto start = std::chrono::steady_clock::now();

  MPI_Init(&argc, &argv);

  int rank{  }, size{  };
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
    float ans = do_parent_work(size, id, a, b, n, i);

    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapse = end - start;
    std::cout << ans << std::endl;
    std::cerr << elapse.count() << std::endl;
  } else {
    // When people get touchy using master/slave
    do_child_work();
  }


	MPI_Finalize();

  return 0;
}
