// Test: [[assume]] attribute (P1774R8)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic assume with simple condition
int test_assume_basic() {
    int x = 5;
    [[assume(x > 0)]];
    return x - 5;
}

// Test case 2: Assume with comparison expression
int test_assume_comparison() {
    int a = 10;
    int b = 20;
    [[assume(a < b)]];
    return b - a - 10;
}

// Test case 3: Assume with boolean variable
int test_assume_bool() {
    bool flag = true;
    [[assume(flag)]];
    return 0;
}

// Test case 4: Multiple assume attributes
int test_multiple_assume() {
    int x = 1;
    [[assume(x > 0)]];
    int y = 2;
    [[assume(y > 0)]];
    return x + y - 3;
}

// Test case 5: Assume with arithmetic expression
int test_assume_arithmetic() {
    int n = 100;
    [[assume(n * 2 > 0)]];
    return n - 100;
}

int main() {
    int r1 = test_assume_basic();
    int r2 = test_assume_comparison();
    int r3 = test_assume_bool();
    int r4 = test_multiple_assume();
    int r5 = test_assume_arithmetic();
    return r1 + r2 + r3 + r4 + r5;
}
