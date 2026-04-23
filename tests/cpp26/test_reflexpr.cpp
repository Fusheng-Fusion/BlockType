// Test: Static Reflection - reflexpr (partial implementation)
// End-to-end compile test for BlockType compiler

// Test case 1: reflexpr on builtin type
int test_reflexpr_builtin() {
    auto info = reflexpr(int);
    return 0;
}

// Test case 2: reflexpr on expression
int test_reflexpr_expr() {
    int x = 42;
    auto info = reflexpr(x);
    return 0;
}

// Test case 3: reflexpr on class type
struct Point {
    int x;
    int y;
    int distance();
};

int test_reflexpr_class() {
    auto info = reflexpr(Point);
    return 0;
}

// Test case 4: __reflect_type built-in
int test_reflect_type() {
    int val = 10;
    auto type_info = __reflect_type(val);
    return 0;
}

// Test case 5: __reflect_members built-in
int test_reflect_members() {
    auto members = __reflect_members(Point);
    return 0;
}

int main() {
    int r1 = test_reflexpr_builtin();
    int r2 = test_reflexpr_expr();
    int r3 = test_reflexpr_class();
    int r4 = test_reflect_type();
    int r5 = test_reflect_members();
    return r1 + r2 + r3 + r4 + r5;
}
