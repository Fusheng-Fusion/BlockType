// Test: Deducing this / Explicit Object Parameter (P0847R7)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic lvalue reference explicit object parameter
struct Doubler {
    int scale(this const Doubler& self) { return 2; }
};

int test_lvalue_ref() {
    Doubler d;
    return d.scale() - 2;
}

// Test case 2: Rvalue reference explicit object parameter
struct Mover {
    int value;
    int take(this Mover&& self) { return self.value; }
};

int test_rvalue_ref() {
    Mover m{42};
    return m.take() - 42;
}

// Test case 3: By-value explicit object parameter
struct Valuer {
    int get(this Valuer self) { return self.value; }
    int value;
};

int test_by_value() {
    Valuer v{10};
    return v.get() - 10;
}

// Test case 4: CRTP-style deducing this
template <typename Derived>
struct Base {
    int get_value(this const Derived& self) {
        return self.impl();
    }
};

struct Concrete : Base<Concrete> {
    int impl() const { return 99; }
};

int test_crtp() {
    Concrete c;
    return c.get_value() - 99;
}

// Test case 5: Const and mutable overloads via deducing this
struct Overloaded {
    int data;
    int read(this const Overloaded& self) { return self.data; }
    int write(this Overloaded& self) { return self.data = 100; }
};

int test_const_mutate() {
    Overloaded o{50};
    int a = o.read();
    o.write();
    int b = o.read();
    return (a - 50) + (b - 100);
}

int main() {
    int r1 = test_lvalue_ref();
    int r2 = test_rvalue_ref();
    int r3 = test_by_value();
    int r4 = test_crtp();
    int r5 = test_const_mutate();
    return r1 + r2 + r3 + r4 + r5;
}
