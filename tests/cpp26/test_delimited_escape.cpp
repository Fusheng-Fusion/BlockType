// Test: Delimited escape sequence \x{...} (P2290R3)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic \x{HH} delimited hex escape
int test_basic_delimited() {
    char c = '\x{41}';
    return 0;
}

// Test case 2: Multi-digit hex escape (Unicode)
int test_unicode_hex() {
    char c = '\x{1F600}';
    return 0;
}

// Test case 3: \x{...} in string literal
int test_delimited_in_string() {
    const char* s = "\x{48}\x{65}\x{6C}\x{6C}\x{6F}";
    return 0;
}

// Test case 4: Minimal \x{0}
int test_zero_delimited() {
    char c = '\x{0}';
    return 0;
}

// Test case 5: \x{...} mixed with regular characters
int test_mixed_escape() {
    const char* s = "A\x{42}C";
    return 0;
}

int main() {
    int r1 = test_basic_delimited();
    int r2 = test_unicode_hex();
    int r3 = test_delimited_in_string();
    int r4 = test_zero_delimited();
    int r5 = test_mixed_escape();
    return r1 + r2 + r3 + r4 + r5;
}
