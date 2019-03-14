#include <type_traits>
#include <iostream>

// If we cant move it then copy it
template <typename T, typename = std::enable_if_t<
	!std::is_nothrow_move_assignable_v<std::remove_reference_t<T>>   and !std::is_nothrow_move_constructible_v<std::remove_reference_t<T>> and
	std::is_nothrow_copy_constructible_v<std::remove_reference_t<T>> and std::is_nothrow_copy_assignable_v<std::remove_reference_t<T>>
>>
constexpr auto move_or_copy(T&& t) -> std::remove_reference_t<T>& {
	return t;
}

// If we can move construct it or move assign it then move it
template <typename T, typename = std::enable_if_t<
	std::is_nothrow_move_constructible_v<std::remove_reference_t<T>> and std::is_nothrow_move_assignable_v<std::remove_reference_t<T>>
>>
constexpr auto move_or_copy(T&& t) -> std::remove_reference_t<T>&& {
	return static_cast<std::remove_reference_t<T>&&>(t);
}

struct Test {
	Test() = default;

	Test(const Test& t) noexcept {
		std::cout << "Copy c'tor" << std::endl;
	}

	Test(Test&& t) noexcept {
		std::cout << "Move c'tor" << std::endl;
		// throw 2;
	}

	Test& operator=(const Test& t) noexcept {
		std::cout << "Copy assign" << std::endl;
		return *this;
	}

	Test& operator=(Test&& t) noexcept {
		std::cout << "Move assign" << std::endl;
		// throw 2;
		return *this;
	}

};

std::ostream& operator<<(std::ostream& out, Test& t) {
	out << "No Optimize" << std::endl;
	return out;
}


int main() {
	std::cout << std::boolalpha;
	std::cout << std::is_nothrow_move_assignable_v<Test> << std::endl;
	std::cout << std::is_nothrow_move_constructible_v<Test> << std::endl;
	std::cout << std::is_nothrow_copy_constructible_v<Test> << std::endl;
	std::cout << std::is_nothrow_copy_assignable_v<Test> << std::endl;
	Test move{};
	Test t = move_or_copy(move);
	std::cout << move << std::endl;
}