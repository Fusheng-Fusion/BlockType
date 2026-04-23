// Test: static operator() (P1169R4) and static operator[] (P2589R1)
// End-to-end compile test for BlockType compiler

// Test case 1: static operator() basic
struct StaticAdder {
    static int operator()(int a, int b) { return a + b; }
};

int test_static_call_op() {
    return StaticAdder::operator()(3, 4) - 7;
}

// Test case 2: static operator[] basic
struct StaticAccessor {
    static int operator[](int idx) { return idx * 2; }
};

int test_static_subscript_op() {
    return StaticAccessor::operator[](5) - 10;
}

// Test case 3: Both static operators in same class
struct MultiStatic {
    static int operator()(int x) { return x + 1; }
    static int operator[](int x) { return x - 1; }
};

int test_both_static_ops() {
    int a = MultiStatic::operator()(10);
    int b = MultiStatic::operator[](10);
    return (a - 11) + (b - 9);
}

// Test case 4: static operator() with different parameter types
struct StaticMultiType {
    static int operator()(int a, double b) { return a; }
};

int test_static_call_multi_type() {
    return StaticMultiType::operator()(5, 3.14);
}

// Test case 5: static operator() called through instance
struct StaticViaInstance {
    static int operator()(int x) { return x * 3; }
};

int test_static_via_instance() {
    StaticViaInstance s;
    return s(4) - 12;
}

int main() {
    int r1 = test_static_call_op();
    int r2 = test_static_subscript_op();
    int r3 = test_both_static_ops();
    int r4 = test_static_call_multi_type();
    int r5 = test_static_via_instance();
    return r1 + r2 + r3 + r4 + r5;
}
