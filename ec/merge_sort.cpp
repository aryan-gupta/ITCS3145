

#include <vector>
#include <random>



constexpr size_t MAX = 1'000;


template <typename I, typename O>
void merge_sort(I begin, I end, O op = std::greater<typename I::value_type>{  }) {

}


int main() {
	std::vector<float> data;

	// I stole code from here: https://stackoverflow.com/questions/19665818
	// I will watch that video later to figure out what this exactally does
	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(1.0, 1'000'000.0);

	for (int i = 0; i < MAX; ++i) {
		data.push_back(dist(mt));
	}

	merge_sort(data.begin(), data.end())
}