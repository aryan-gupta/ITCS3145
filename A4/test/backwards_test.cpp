
#include <iostream>
#include <functional>
#include <type_traits>
#include <typeinfo>
#ifndef _MSC_VER
#   include <cxxabi.h>
#endif
#include <memory>
#include <string>
#include <cstdlib>

/// STOLEN FROM https://stackoverflow.com/questions/81870
template <class T>
std::string
type_name()
{
    typedef typename std::remove_reference<T>::type TR;
    std::unique_ptr<char, void(*)(void*)> own
           (
#ifndef _MSC_VER
                abi::__cxa_demangle(typeid(TR).name(), nullptr,
                                           nullptr, nullptr),
#else
                nullptr,
#endif
                std::free
           );
    std::string r = own != nullptr ? own.get() : typeid(TR).name();
    if (std::is_const<TR>::value)
        r += " const";
    if (std::is_volatile<TR>::value)
        r += " volatile";
    if (std::is_lvalue_reference<T>::value)
        r += "&";
    else if (std::is_rvalue_reference<T>::value)
        r += "&&";
    return r;
}

template <typename T>
void test(T& value) {
	std::cout << "I am lvalue\n";
}

template <typename T>
void test(T&& value) {
	std::cout << "I am rvalue\n";
}

template <typename T>
void test(const T& value) {
	std::cout << "I am const lvalue\n";
}

/// Was watching https://youtu.be/hwT8K3-NH1w . I want to try this
// If T is a reference then the cast will collapse to a reference
// If T is a base type, then the cast will collapse to a rvalue
template <typename T>
T&& backwards(T& value) {
  return static_cast<T&&>(value);
}

// template <typename T>
// T&& backwards(typename std::remove_reference<T>::type& value) {
//   return static_cast<T&&>(value);
// }

struct int_wrapper{
	int a;
	int_wrapper() {
		a = 5;
	}
};

int a = 5;
int& get_ref() {
	return a;
}

const int& get_cref() {
	return a;
}

int get_val() {
	return a;
}

template <typename F, typename R = std::invoke_result_t<F>>
R test_this(F func) {
	R val = func();
	return val;
}

int main() {
	test( test_this(&get_ref) );
	// std::cout << type_name<decltype(x)>() << std::endl;
	test( test_this(&get_val) );
	// std::cout << type_name<decltype(y)>() << std::endl;
	test( test_this(&get_cref) );
	// std::cout << type_name<decltype(z)>() << std::endl;
}