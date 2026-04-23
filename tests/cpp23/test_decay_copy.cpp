// Test: decay-copy auto(x) / auto{x} (P0849R8)
// End-to-end compile test for BlockType compiler

// Test case 1: auto(x) with integer
int test_auto_paren_int() {
    int x = 10;
    int y = auto(x);
    return y - 10;
}

// Test case 2: auto{x} with integer (brace initialization)
int test_auto_brace_int() {
    int x = 20;
    int y = auto{x};
    return y - 20;
}

// Test case 3: auto(x) with different types
int test_auto_paren_expr() {
    double d = 3.14;
    auto copy = auto(d);
    return 0;
}

// Test case 4: auto(x) in return context
int test_auto_in_return() {
    int value = 42;
    return auto(value) - 42;
}

// Test case 5: auto(x) in initialization
int test_auto_init() {
    int a = 5;
    int b = auto(a + 3);
    return b - 8;
}

int main() {
    int r1 = test_auto_paren_int();
    int r2 = test_auto_brace_int();
    int r3 = test_auto_paren_expr();
    int r4 = test_auto_in_return();
    int r5 = test_auto_init();
    return r1 + r2 + r3 + r4 + r5;
}
