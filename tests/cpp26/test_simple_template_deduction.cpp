// Simplified test for template argument deduction and auto return type
// This tests the core functionality without variadic templates

template<class T1, class T2>
struct MyPair {
    T1 first;
    T2 second;
};

// Factory function with auto return type
template<class T1, class T2>
auto make_my_pair(T1 a, T2 b) {
    return MyPair<T1, T2>{a, b};
}

void test_factory_function() {
    // Test factory function with template argument deduction
    auto p1 = make_my_pair(42, 3.14);
    auto [x1, y1] = p1;
    
    // Test with different types
    auto p2 = make_my_pair('A', 100);
    auto [x2, y2] = p2;
}

int main() {
    test_factory_function();
    return 0;
}
