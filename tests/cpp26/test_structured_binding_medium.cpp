// Medium complexity structured binding test
// Tests brace initialization and multiple scenarios

// Test 1: Array with brace initialization
void test_brace_init() {
    int arr[2] = {10, 20};
    auto [a, b] = arr;
    
    if (a != 10) return;
    if (b != 20) return;
}

// Test 2: Three-element array
void test_three_elem() {
    int data[3] = {100, 200, 300};
    auto [x, y, z] = data;
    
    if (x != 100) return;
    if (y != 200) return;
    if (z != 300) return;
}

// Test 3: Multiple bindings in one function
void test_multiple() {
    int arr1[2] = {1, 2};
    int arr2[3] = {3, 4, 5};
    
    auto [p, q] = arr1;
    auto [r, s, t] = arr2;
    
    if (p != 1 || q != 2) return;
    if (r != 3 || s != 4 || t != 5) return;
}

// Test 4: Binding in nested scope
void test_nested_scope() {
    {
        int inner[2] = {42, 99};
        auto [first, second] = inner;
        
        if (first != 42) return;
        if (second != 99) return;
    }
}

// Test 5: Binding with computation
void test_computation() {
    int values[2] = {50, 60};
    auto [v1, v2] = values;
    
    int sum = v1 + v2;
    if (sum != 110) return;
}

int main() {
    test_brace_init();
    test_three_elem();
    test_multiple();
    test_nested_scope();
    test_computation();
    
    return 0;
}
