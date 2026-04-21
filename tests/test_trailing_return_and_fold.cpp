// Test trailing return type and fold expression integration
// Compile: clang++ -std=c++17 test_trailing_return_and_fold.cpp

#include <iostream>
#include <vector>

// Test 1: Trailing return type (C++11)
auto add(int a, int b) -> int {
    return a + b;
}

auto multiply(double a, double b) -> double {
    return a * b;
}

// Test 2: Trailing return type with auto deduction
auto getVector() -> std::vector<int> {
    return {1, 2, 3, 4, 5};
}

// Test 3: Lambda with trailing return type
auto lambda = []() -> int { return 42; };

// Test 4: Fold expressions (C++17)
template<typename... Args>
auto sum(Args... args) {
    return (args + ...);  // Unary right fold
}

template<typename... Args>
auto product(Args... args) {
    return (... * args);  // Unary left fold
}

template<typename... Args>
auto sum_with_init(Args... args) {
    return (args + ... + 0);  // Binary right fold
}

int main() {
    // Test trailing return type
    std::cout << "add(3, 4) = " << add(3, 4) << std::endl;
    std::cout << "multiply(2.5, 3.0) = " << multiply(2.5, 3.0) << std::endl;
    
    auto vec = getVector();
    std::cout << "Vector size: " << vec.size() << std::endl;
    
    std::cout << "Lambda: " << lambda() << std::endl;
    
    // Test fold expressions
    std::cout << "sum(1, 2, 3, 4, 5) = " << sum(1, 2, 3, 4, 5) << std::endl;
    std::cout << "product(1, 2, 3, 4) = " << product(1, 2, 3, 4) << std::endl;
    std::cout << "sum_with_init() = " << sum_with_init() << std::endl;
    std::cout << "sum_with_init(1, 2, 3) = " << sum_with_init(1, 2, 3) << std::endl;
    
    return 0;
}
