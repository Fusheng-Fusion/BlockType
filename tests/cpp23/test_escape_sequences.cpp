// Test: Escape sequences - \e (P2314R4)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic \e escape
int test_e_escape() {
    char c = '\e';
    return 0;
}

// Test case 2: \E uppercase escape
int test_E_escape() {
    char c = '\E';
    return 0;
}

// Test case 3: \e in string literal
int test_e_in_string() {
    const char* s = "\e";
    return 0;
}

// Test case 4: \e value check (ESC = 0x1B = 27)
int test_e_value() {
    char c = '\e';
    return (c == 27) ? 0 : 1;
}

// Test case 5: \e combined with other escapes
int test_e_combined() {
    const char* s = "\e\n\t";
    return 0;
}

int main() {
    int r1 = test_e_escape();
    int r2 = test_E_escape();
    int r3 = test_e_in_string();
    int r4 = test_e_value();
    int r5 = test_e_combined();
    return r1 + r2 + r3 + r4 + r5;
}
