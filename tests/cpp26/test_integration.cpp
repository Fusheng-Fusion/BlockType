// Integration Test: Combining multiple C++23/C++26 features
// End-to-end compile test for BlockType compiler

// Integration 1: Pack Indexing + constexpr
template <typename... Ts>
constexpr int pack_size() { return sizeof...(Ts); }

template <typename... Ts>
using FirstOf = Ts...[0];

int test_pack_constexpr() {
    constexpr int sz = pack_size<int, double, char>();
    FirstOf<int, double, char> val = 42;
    return val - 42;
}

// Integration 2: Deducing this + template
template <typename T>
struct Wrapper {
    T value;
    T get(this const Wrapper& self) { return self.value; }
};

int test_deducing_this_template() {
    Wrapper<int> w{99};
    return w.get() - 99;
}

// Integration 3: Lambda template + constexpr
int test_lambda_template_constexpr() {
    constexpr auto f = []<typename T>(T x) { return x; };
    return f(7) - 7;
}

// Integration 4: static operator() + template
template <typename T>
struct GenericConverter {
    static T operator()(T val) { return val; }
};

int test_static_op_template() {
    return GenericConverter<int>::operator()(5) - 5;
}

// Integration 5: if consteval + deducing this
struct ConstEvalChecker {
    int check(this ConstEvalChecker& self) {
        if consteval {
            return 1;
        } else {
            return 0;
        }
    }
};

int test_consteval_deducing_this() {
    ConstEvalChecker c;
    return c.check();
}

// Integration 6: Multi-dim subscript + constexpr
struct ConstExprMatrix {
    constexpr int operator[](int r, int c) const { return r * 10 + c; }
};

int test_constexpr_multidim() {
    ConstExprMatrix m;
    return m[2, 3] - 23;
}

// Integration 7: [[assume]] + deducing this
struct Assumer {
    int compute(this const Assumer& self, int n) {
        [[assume(n > 0)]];
        return n;
    }
};

int test_assume_deducing_this() {
    Assumer a;
    return a.compute(10) - 10;
}

// Integration 8: Lambda template + pack indexing
int test_lambda_pack_index() {
    auto f = []<typename... Ts>(Ts...[0] first) { return first; };
    return f(42) - 42;
}

int main() {
    int r1 = test_pack_constexpr();
    int r2 = test_deducing_this_template();
    int r3 = test_lambda_template_constexpr();
    int r4 = test_static_op_template();
    int r5 = test_consteval_deducing_this();
    int r6 = test_constexpr_multidim();
    int r7 = test_assume_deducing_this();
    int r8 = test_lambda_pack_index();
    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8;
}
