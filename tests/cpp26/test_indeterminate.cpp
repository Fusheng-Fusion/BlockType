// Test: [[indeterminate]] attribute (P2795R5)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic [[indeterminate]] on int
int test_indeterminate_int() {
    [[indeterminate]] int x;
    return 0;
}

// Test case 2: [[indeterminate]] on array
int test_indeterminate_array() {
    [[indeterminate]] int arr[4];
    return 0;
}

// Test case 3: [[indeterminate]] on struct member context
struct Data {
    int value;
};

int test_indeterminate_struct() {
    [[indeterminate]] Data d;
    return 0;
}

// Test case 4: [[indeterminate]] with initialization override
int test_indeterminate_init() {
    [[indeterminate]] int x = 42;
    return x - 42;
}

int main() {
    int r1 = test_indeterminate_int();
    int r2 = test_indeterminate_array();
    int r3 = test_indeterminate_struct();
    int r4 = test_indeterminate_init();
    return r1 + r2 + r3 + r4;
}
