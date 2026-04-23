// Test: Lambda enhancements - template parameters (P1102R2) and attributes (P2173R1)
// End-to-end compile test for BlockType compiler

// Test case 1: Lambda with explicit template parameter
int test_lambda_template() {
    auto f = []<typename T>(T x) { return x; };
    return f(42) - 42;
}

// Test case 2: Lambda with multiple template parameters
int test_lambda_multi_template() {
    auto add = []<typename T, typename U>(T a, U b) { return a + b; };
    return add(3, 4) - 7;
}

// Test case 3: Lambda with leading attribute
int test_lambda_leading_attr() {
    [[nodiscard]] auto f = []() { return 1; };
    return f() - 1;
}

// Test case 4: Lambda with template and trailing attribute
int test_lambda_template_attr() {
    auto f = []<typename T>(T x) [[nodiscard]] { return x; };
    return f(10) - 10;
}

// Test case 5: Lambda template with non-type parameter
int test_lambda_nontype_template() {
    auto f = []<int N>() { return N; };
    return f<5>() - 5;
}

// Test case 6: Nested lambda with template
int test_nested_lambda_template() {
    auto outer = []<typename T>(T x) {
        auto inner = []<typename U>(U y) { return y; };
        return inner(x);
    };
    return outer(7) - 7;
}

int main() {
    int r1 = test_lambda_template();
    int r2 = test_lambda_multi_template();
    int r3 = test_lambda_leading_attr();
    int r4 = test_lambda_template_attr();
    int r5 = test_lambda_nontype_template();
    int r6 = test_nested_lambda_template();
    return r1 + r2 + r3 + r4 + r5 + r6;
}
