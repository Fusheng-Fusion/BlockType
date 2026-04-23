// Test: Multi-dimensional operator[] (P2128R6)
// End-to-end compile test for BlockType compiler

// Test case 1: 2D matrix with operator[]
struct Matrix2D {
    int data[4];
    int& operator[](int r, int c) { return data[r * 2 + c]; }
};

int test_2d_subscript() {
    Matrix2D m;
    m[0, 0] = 1;
    m[0, 1] = 2;
    m[1, 0] = 3;
    m[1, 1] = 4;
    return m[0, 0] + m[1, 1] - 5;
}

// Test case 2: 3D array with operator[]
struct Tensor3D {
    int data[8];
    int& operator[](int x, int y, int z) { return data[x * 4 + y * 2 + z]; }
};

int test_3d_subscript() {
    Tensor3D t;
    t[0, 0, 0] = 10;
    t[1, 1, 1] = 20;
    return t[0, 0, 0] + t[1, 1, 1] - 30;
}

// Test case 3: Multi-dim subscript with different index types
struct MixedIndex {
    int data[6];
    int& operator[](int i, int j) { return data[i * 3 + j]; }
};

int test_mixed_subscript() {
    MixedIndex mi;
    mi[0, 0] = 100;
    mi[1, 2] = 200;
    return mi[0, 0] + mi[1, 2] - 300;
}

// Test case 4: Const multi-dim subscript
struct ConstMatrix {
    int data[4];
    const int& operator[](int r, int c) const { return data[r * 2 + c]; }
};

int test_const_subscript() {
    ConstMatrix cm = {{1, 2, 3, 4}};
    return cm[0, 0] - 1;
}

// Test case 5: Multi-dim subscript returning value (not reference)
struct FlatMatrix {
    int get(int r, int c) { return r * 10 + c; }
    int operator[](int r, int c) { return get(r, c); }
};

int test_value_subscript() {
    FlatMatrix fm;
    return fm[2, 3] - 23;
}

int main() {
    int r1 = test_2d_subscript();
    int r2 = test_3d_subscript();
    int r3 = test_mixed_subscript();
    int r4 = test_const_subscript();
    int r5 = test_value_subscript();
    return r1 + r2 + r3 + r4 + r5;
}
