// Test: Contract mode switching (P2900R14) - P7.3.2
// End-to-end compile test for BlockType compiler
// This file tests that the compiler accepts contract mode options.
// The actual mode behavior is tested via CompilerInvocation unit tests.

// Test case 1: Function with contract (mode is set via compiler flag)
[[pre: x > 0]]
int test_enforce(int x) {
    return x;
}

// Test case 2: Function with post contract
[[post: result >= 0]]
int test_observe(int x) {
    return x * x;
}

// Test case 3: Multiple contracts
[[pre: a > 0]]
[[post: result > 0]]
int test_multiple(int a) {
    return a + 1;
}

// Test case 4: Contract with assert
int test_assert_mode(int x) {
    [[assert: x > 0]];
    return x;
}

int main() {
    int r1 = test_enforce(5);
    int r2 = test_observe(3);
    int r3 = test_multiple(2);
    int r4 = test_assert_mode(7);
    return r1 + r2 + r3 + r4 - 22;
}
