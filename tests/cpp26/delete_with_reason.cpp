// Test: = delete("reason") - C++26 P2573R2
// 测试带原因的deleted函数

class NonCopyable {
public:
    NonCopyable() = default;
    
    // 带原因的deleted拷贝构造函数
    NonCopyable(const NonCopyable &) = delete("NonCopyable objects cannot be copied");
    
    // 带原因的deleted赋值运算符
    NonCopyable &operator=(const NonCopyable &) = delete("Assignment is not allowed");
};

void test_delete_with_reason() {
    NonCopyable obj1;
    
    // 应该报错并显示自定义原因
    NonCopyable obj2 = obj1;  // error: call to deleted function 'NonCopyable': NonCopyable objects cannot be copied
    
    // 应该报错并显示自定义原因
    NonCopyable obj3;
    obj3 = obj1;  // error: call to deleted function 'operator=': Assignment is not allowed
}

// 测试不带原因的delete（向后兼容）
class LegacyDeleted {
public:
    LegacyDeleted() = default;
    LegacyDeleted(const LegacyDeleted &) = delete;  // 传统delete，无原因
};

void test_legacy_delete() {
    LegacyDeleted a;
    LegacyDeleted b = a;  // error: call to deleted function 'LegacyDeleted'
}

// 测试空字符串原因
void empty_reason() = delete("");

// 测试自由函数带原因的delete
void deprecated_func() = delete("Use new_func() instead");

void test_free_function_delete() {
    deprecated_func();  // error: call to deleted function 'deprecated_func': Use new_func() instead
}

// P7.4.1 边界测试：非字符串字面量 delete reason（应报错）
// int x;
// void bad_delete_int() = delete(x);       // error: delete reason must be a string literal
// void bad_delete_number() = delete(42);   // error: delete reason must be a string literal

int main() {
    return 0;
}
