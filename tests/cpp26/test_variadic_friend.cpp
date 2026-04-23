// Test: Variadic friend declarations (P2893R3)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic variadic friend with parameter pack
template <typename... Ts>
struct FriendHolder {
    friend Ts...;
};

int test_basic_variadic_friend() {
    FriendHolder<int, double> fh;
    return 0;
}

// Test case 2: Variadic friend with single type
template <typename... Ts>
struct SingleFriend {
    friend Ts...;
};

int test_single_variadic_friend() {
    SingleFriend<int> sf;
    return 0;
}

// Test case 3: Variadic friend in nested template
template <typename... Outer>
struct OuterWrap {
    template <typename... Inner>
    struct InnerWrap {
        friend Outer...;
        friend Inner...;
    };
};

int test_nested_variadic_friend() {
    OuterWrap<int, double>::InnerWrap<char> iw;
    return 0;
}

int main() {
    int r1 = test_basic_variadic_friend();
    int r2 = test_single_variadic_friend();
    int r3 = test_nested_variadic_friend();
    return r1 + r2 + r3;
}
