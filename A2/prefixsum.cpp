
#include <iostream>

void prefixsum(int* arr, int n, int* pr) {
	pr[0] = 0;
	for (int i = 1; i <= n; ++i) {
		pr[i] = pr[i - 1] + arr[i - 1];
	}
}


int main() {
	int* ps = new int[9];
	int* ar = new int[8];

	for (int i = 0; i < 8; ++i)
		ar[i] = i;

	prefixsum(ar, 8, ps);

	for (int i = 0; i < 9; ++i)
		std::cout << ps[i] << " " << std::endl;
}