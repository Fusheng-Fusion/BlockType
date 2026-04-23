// Test: Literal suffixes - Z/z for size_t (P0330R8)
// End-to-end compile test for BlockType compiler

// Test case 1: uz suffix
int test_uz_suffix() {
    auto x = 42uz;
    return 0;
}

// Test case 2: zu suffix
int test_zu_suffix() {
    auto x = 100zu;
    return 0;
}

// Test case 3: UZ and ZU uppercase
int test_uppercase_suffix() {
    auto x = 42UZ;
    auto y = 100ZU;
    return 0;
}

// Test case 4: uz in expression
int test_uz_expression() {
    auto a = 10uz;
    auto b = 20uz;
    return 0;
}

// Test case 5: uz in array size context
int test_uz_array() {
    auto n = 5uz;
    return 0;
}

int main() {
    int r1 = test_uz_suffix();
    int r2 = test_zu_suffix();
    int r3 = test_uppercase_suffix();
    int r4 = test_uz_expression();
    int r5 = test_uz_array();
    return r1 + r2 + r3 + r4 + r5;
}
