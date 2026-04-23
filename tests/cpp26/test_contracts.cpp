// Test: Contracts (P2900R14) - pre/post/assert
// End-to-end compile test for BlockType compiler

// Test case 1: [[pre:]] precondition contract
[[pre: x > 0]]
int test_precondition(int x) {
    return x + 1;
}

// Test case 2: [[post:]] postcondition contract
[[post: result > 0]]
int test_postcondition(int x) {
    return x * 2;
}

// Test case 3: [[assert:]] assertion contract
int test_assertion(int x) {
    [[assert: x > 0]];
    return x;
}

// Test case 4: Multiple contracts on same function
[[pre: x >= 0]]
[[post: result >= 0]]
int test_multiple_contracts(int x) {
    return x * x;
}

// Test case 5: Contract with pointer
[[pre: ptr != nullptr]]
int test_ptr_contract(int *ptr) {
    return *ptr;
}

// Test case 6: Contract with boolean expression
[[pre: a > 0 && b > 0]]
int test_bool_contract(int a, int b) {
    return a + b;
}

// Test case 7: Postcondition with result keyword
[[post: result == x + 1]]
int test_result_keyword(int x) {
    return x + 1;
}

// Test case 8: Assert in nested scope
int test_nested_assert(int x) {
    if (x > 0) {
        [[assert: x > 0]];
        return x;
    }
    return -x;
}

int main() {
    int r1 = test_precondition(5);
    int r2 = test_postcondition(3);
    int r3 = test_assertion(7);
    int r4 = test_multiple_contracts(4);
    int v = 42;
    int r5 = test_ptr_contract(&v);
    int r6 = test_bool_contract(1, 2);
    int r7 = test_result_keyword(10);
    int r8 = test_nested_assert(5);
    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 - 116;
}
