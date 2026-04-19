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

int main() {
    return 0;
}
