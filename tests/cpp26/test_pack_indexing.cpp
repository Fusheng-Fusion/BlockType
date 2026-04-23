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

int main() {
    int r1 = test_basic_pack_index();
    int r2 = test_last_pack_index();
    int r3 = test_pack_index_func();
    int r4 = test_pack_index_struct();
    int r5 = test_multi_pack_index();
    int r6 = test_pack_index_three();
    int r7 = test_pack_var_decl();
    return r1 + r2 + r3 + r4 + r5 + r6 + r7;
}
