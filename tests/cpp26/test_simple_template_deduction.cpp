// Simple test for template argument deduction
template<class T1, class T2>
struct MyPair {
    T1 first;
    T2 second;
};

// Factory function
template<class T1, class T2>
MyPair<T1, T2> make_my_pair(T1 a, T2 b) {
    return MyPair<T1, T2>{a, b};
}

void test_template_deduction() {
    // Test template argument deduction
    auto p = make_my_pair(42, 3.14);
}

int main() {
    test_template_deduction();
    return 0;
}
