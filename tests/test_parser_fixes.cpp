// Test parser fixes for three issues
// Compile: clang++ -std=c++20 test_parser_fixes.cpp

#include <iostream>
#include <tuple>
#include <string>

// Test 1: Structured binding with reference qualifiers
void test_structured_binding() {
    std::cout << "\n=== Test 1: Structured Binding ===\n";
    
    auto pair = std::make_pair(42, 3.14);
    
    // Test 1.1: Basic structured binding
    auto [x, y] = pair;
    std::cout << "auto [x, y] = pair: x=" << x << ", y=" << y << std::endl;
    
    // Test 1.2: Lvalue reference
    auto& [rx, ry] = pair;
    std::cout << "auto& [rx, ry] = pair: rx=" << rx << ", ry=" << ry << std::endl;
    
    // Test 1.3: Rvalue reference
    auto&& [rrx, rry] = std::make_pair(100, 200.0);
    std::cout << "auto&& [rrx, rry]: rrx=" << rrx << ", rry=" << rry << std::endl;
}

// Test 2: Error recovery
void test_error_recovery() {
    std::cout << "\n=== Test 2: Error Recovery ===\n";
    
    // This should trigger error recovery but not skip too much
    int a = 10  // Missing semicolon - should recover
    int b = 20;
    std::cout << "After error recovery: a=" << a << ", b=" << b << std::endl;
    
    // Another error case
    int c = 30;
    // Missing expression after =
    // int d = ;  // This would be caught
    int e = 40;
    std::cout << "Multiple errors handled: c=" << c << ", e=" << e << std::endl;
}

// Test 3: Complex using declarations
namespace Outer {
    namespace Inner {
        struct Point {
            int x, y;
        };
        
        void freeFunction() { std::cout << "Outer::Inner::freeFunction\n"; }
        
        template<typename T>
        struct Template {
            T value;
        };
    }
}

void test_using_declarations() {
    std::cout << "\n=== Test 3: Using Declarations ===\n";
    
    // Test 3.1: Simple using declaration
    using Outer::Inner::Point;
    Point p{1, 2};
    std::cout << "using Outer::Inner::Point: p.x=" << p.x << ", p.y=" << p.y << std::endl;
    
    // Test 3.2: Using function
    using Outer::Inner::freeFunction;
    freeFunction();
    
    // Test 3.3: Using template
    using Outer::Inner::Template;
    Template<int> t{42};
    std::cout << "using Outer::Inner::Template: t.value=" << t.value << std::endl;
    
    // Test 3.4: Using with typename
    using typename Outer::Inner::Point;
    
    // Test 3.5: Using operator (commented as it requires operator overloading)
    // using Outer::Inner::operator+;
}

int main() {
    std::cout << "Testing Parser Fixes\n";
    std::cout << "====================\n";
    
    test_structured_binding();
    test_error_recovery();
    test_using_declarations();
    
    std::cout << "\n=== All tests completed ===\n";
    return 0;
}
