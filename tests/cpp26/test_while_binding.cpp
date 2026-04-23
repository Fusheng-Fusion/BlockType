// Test: while condition with structured binding - C++26 P0963R3
// 测试 while 条件中的结构化绑定

#include <tuple>

// Test case 1: Basic while with structured binding
void test_while_binding_basic() {
    std::pair<int, double> p{1, 2.5};
    while (auto [x, y] = p) {
        (void)x;
        (void)y;
        break;  // prevent infinite loop
    }
}

// Test case 2: while with placeholder binding
void test_while_binding_placeholder() {
    std::pair<int, double> p{1, 2.5};
    while (auto [_, value] = p) {
        (void)value;
        break;
    }
}

int main() {
    test_while_binding_basic();
    test_while_binding_placeholder();
    return 0;
}
