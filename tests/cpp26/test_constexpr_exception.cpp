// Test: constexpr exceptions (P3068R6)
// End-to-end compile test for BlockType compiler

// Test case 1: constexpr function with throw that is never reached
constexpr int safe_divide(int a, int b) {
    if (b == 0) throw 1;
    return a / b;
}

int test_constexpr_nothrow() {
    constexpr int result = safe_divide(10, 2);
    return result - 5;
}

// Test case 2: constexpr function with throw expression
constexpr int maybe_throw(bool b) {
    if (b) throw 42;
    return 0;
}

int test_constexpr_throw_not_executed() {
    constexpr int val = maybe_throw(false);
    return val;
}

// Test case 3: constexpr function with throw in nested call
constexpr int inner(int x) {
    if (x < 0) throw -1;
    return x * 2;
}

constexpr int outer(int x) {
    return inner(x) + 1;
}

int test_constexpr_nested_throw() {
    constexpr int result = outer(5);
    return result - 11;
}

// Test case 4: Multiple throw points in constexpr
constexpr int multi_throw(int n) {
    if (n < 0) throw 1;
    if (n > 100) throw 2;
    return n;
}

int test_constexpr_multi_throw() {
    constexpr int a = multi_throw(50);
    return a - 50;
}

int main() {
    int r1 = test_constexpr_nothrow();
    int r2 = test_constexpr_throw_not_executed();
    int r3 = test_constexpr_nested_throw();
    int r4 = test_constexpr_multi_throw();
    return r1 + r2 + r3 + r4;
}
