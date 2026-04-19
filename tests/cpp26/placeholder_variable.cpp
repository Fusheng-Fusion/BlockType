// Test: Placeholder variable `_` - C++26 P2169R4
// 测试未命名占位符变量

#include <tuple>

void test_placeholder_basic() {
    // 基本用法：多个_在同一作用域
    auto _ = 42;
    auto _ = 3.14;  // OK: 每个_都是新变量
    auto _ = "hello";  // OK: 不冲突
    
    // _ 不应该产生 "unused variable" 警告
}

void test_placeholder_structured_binding() {
    std::pair<int, double> p{1, 2.5};
    
    // 结构化绑定中使用_忽略某些值
    auto [_, value] = p;  // 忽略第一个元素
    (void)value;
    
    auto [first, _] = p;  // 忽略第二个元素
    (void)first;
}

void test_placeholder_range_for() {
    int arr[] = {1, 2, 3};
    
    // 范围for中忽略索引
    for (auto _ : arr) {
        // 只关心循环次数，不关心具体值
    }
}

void test_placeholder_with_init() {
    // 直接初始化
    int _(10);
    
    // 列表初始化
    int _{20};
}

// 测试：_不应该被加入符号表
void test_placeholder_no_symbol() {
    auto _ = 100;
    // auto x = _;  // Error: _ is not in symbol table
    // 上面的代码应该报错，因为_不在符号表中
}

int main() {
    test_placeholder_basic();
    test_placeholder_structured_binding();
    test_placeholder_range_for();
    test_placeholder_with_init();
    test_placeholder_no_symbol();
    return 0;
}
