// Test: Trivially relocatable trait (P2786R13)
// End-to-end compile test for BlockType compiler

// Test case 1: __is_trivially_relocatable on simple struct
struct SimpleStruct {
    int x;
    double y;
};

int test_trivially_relocatable_simple() {
    bool b = __is_trivially_relocatable(SimpleStruct);
    return 0;
}

// Test case 2: __is_trivially_relocatable on basic types
int test_trivially_relocatable_basic() {
    bool b1 = __is_trivially_relocatable(int);
    bool b2 = __is_trivially_relocatable(double);
    return 0;
}

// Test case 3: __is_trivially_relocatable with user-defined destructor
struct WithDtor {
    int x;
    ~WithDtor() {}
};

int test_trivially_relocatable_with_dtor() {
    bool b = __is_trivially_relocatable(WithDtor);
    return 0;
}

// Test case 4: __is_trivially_relocatable on pointer types
int test_trivially_relocatable_ptr() {
    bool b = __is_trivially_relocatable(int*);
    return 0;
}

int main() {
    int r1 = test_trivially_relocatable_simple();
    int r2 = test_trivially_relocatable_basic();
    int r3 = test_trivially_relocatable_with_dtor();
    int r4 = test_trivially_relocatable_ptr();
    return r1 + r2 + r3 + r4;
}
