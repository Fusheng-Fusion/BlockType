// Test: Pack Indexing T...[N] (P2662R3)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic pack indexing with type alias
template <typename... Ts>
using FirstType = Ts...[0];

int test_basic_pack_index() {
    return 0;
}

// Test case 2: Pack indexing to get last element
template <typename... Ts>
using LastType = Ts...[sizeof...(Ts) - 1];

int test_last_pack_index() {
    return 0;
}

// Test case 3: Pack indexing in function parameter
template <typename... Ts>
int get_at_index() {
    return 0;
}

int test_pack_index_func() {
    return get_at_index<int, double, char>();
}

// Test case 4: Pack indexing with constant expression index
template <typename... Ts>
struct TypeList {
    template <int N>
    using Type = Ts...[N];
};

int test_pack_index_struct() {
    TypeList<int, double, char>::Type<0> x = 42;
    return x - 42;
}

// Test case 5: Multiple pack indexing in same context
template <typename... Ts>
struct MultiIndex {
    using First = Ts...[0];
    using Second = Ts...[1];
};

int test_multi_pack_index() {
    MultiIndex<int, double>::First a = 10;
    return a - 10;
}

// Test case 6: Pack indexing with 3+ types
template <typename... Ts>
using ThirdType = Ts...[2];

int test_pack_index_three() {
    return 0;
}

// Test case 7: Pack indexing in variable declaration
template <typename... Ts>
void pack_var() {
    Ts...[0] var{};
}

int test_pack_var_decl() {
    pack_var<int, double>();
    return 0;
}

// Test case 8: Nested pack indexing
template <typename... Ts>
struct NestedPackIndex {
    using Inner = Ts...[0];
    template <typename... Us>
    struct InnerPack {
        using Combined = Us...[0];
    };
};

int test_nested_pack_index() {
    return 0;
}

// Test case 9: Pack indexing in return type
template <typename... Ts>
auto get_first_type() -> Ts...[0] {
    return Ts...[0]{};
}

int test_return_type_pack_index() {
    return 0;
}

// Test case 10: Pack indexing with sizeof...(Ts) - 1 (last element)
template <typename... Ts>
struct LastElement {
    using Type = Ts...[sizeof...(Ts) - 1];
};

int test_last_element() {
    return 0;
}

// P7.4.4 边界测试：负索引（应报错）
// template <typename... Ts>
// using NegativeIndex = Ts...[-1];  // error: pack index must not be negative

// P7.4.4 边界测试：0 - 1 模式的负索引
// template <typename... Ts>
// using ZeroMinusOne = Ts...[0 - 1];  // error: pack index must not be negative

int main() {
    int r1 = test_basic_pack_index();
    int r2 = test_last_pack_index();
    int r3 = test_pack_index_func();
    int r4 = test_pack_index_struct();
    int r5 = test_multi_pack_index();
    int r6 = test_pack_index_three();
    int r7 = test_pack_var_decl();
    int r8 = test_nested_pack_index();
    int r9 = test_return_type_pack_index();
    int r10 = test_last_element();
    return r1 + r2 + r3 + r4 + r5 + r6 + r7 + r8 + r9 + r10;
}
