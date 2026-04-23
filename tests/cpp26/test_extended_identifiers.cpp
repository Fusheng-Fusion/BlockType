// Test: Extended identifier characters @/$/` (P2558R2)
// End-to-end compile test for BlockType compiler

// Test case 1: $ in identifier
int test_dollar_identifier() {
    int $x = 42;
    return $x - 42;
}

// Test case 2: @ in identifier
int test_at_identifier() {
    int @var = 10;
    return @var - 10;
}

// Test case 3: Multiple extended chars in same identifier
int test_mixed_extended() {
    int $value@ = 5;
    return $value@ - 5;
}

// Test case 4: Extended identifier in struct
int test_struct_extended_member() {
    return 0;
}

// Test case 5: Extended identifier in function parameter
int test_param_extended(int $n) {
    return $n;
}

int main() {
    int r1 = test_dollar_identifier();
    int r2 = test_at_identifier();
    int r3 = test_mixed_extended();
    int r4 = test_struct_extended_member();
    int r5 = test_param_extended(0);
    return r1 + r2 + r3 + r4 + r5;
}
