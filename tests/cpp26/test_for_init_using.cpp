// Test: for init-statement using (P2360R0)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic for-init using declaration
int test_basic_for_using() {
    for (using T = int; T x = 0;) {
        break;
    }
    return 0;
}

// Test case 2: for-init using with type alias
int test_for_using_alias() {
    for (using IntType = int; IntType i = 5; ) {
        break;
    }
    return 0;
}

// Test case 3: for-init using with multiple iterations
int test_for_using_loop() {
    int count = 0;
    for (using T = int; T i = 0; ) {
        count = count + 1;
        break;
    }
    return count - 1;
}

int main() {
    int r1 = test_basic_for_using();
    int r2 = test_for_using_alias();
    int r3 = test_for_using_loop();
    return r1 + r2 + r3;
}
