void test_const_binding() {
    const int arr[2] = {42, 99};
    auto [a, b] = arr;
    
    if (a != 42) return;
    if (b != 99) return;
}

int main() {
    test_const_binding();
    return 0;
}
