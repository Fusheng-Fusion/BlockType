void test_for_no_decl() {
    int i = 0;
    for (; i < 3; ++i) {
        int x = i;
    }
}

int main() {
    test_for_no_decl();
    return 0;
}
