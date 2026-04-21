// Test co_await expression and member pointer type integration
// Compile: clang++ -std=c++20 test_coawait_and_member_pointer.cpp

#include <iostream>
#include <coroutine>

// Test 1: Member pointer type
class MyClass {
public:
    int x;
    double y;
    
    void method() { std::cout << "method called" << std::endl; }
    int getValue() { return x; }
};

// Member pointer declarations
int MyClass::*intMemberPtr = &MyClass::x;
double MyClass::*doubleMemberPtr = &MyClass::y;
void (MyClass::*methodPtr)() = &MyClass::method;
int (MyClass::*getValuePtr)() = &MyClass::getValue;

// Test 2: Coroutine with co_await
struct Task {
    struct promise_type {
        Task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

struct Awaiter {
    bool await_ready() { return false; }
    void await_suspend(std::coroutine_handle<>) {}
    void await_resume() { std::cout << "Awaiter resumed" << std::endl; }
};

Awaiter getAwaitable() { return {}; }

Task testCoroutine() {
    std::cout << "Before co_await" << std::endl;
    co_await getAwaitable();  // co_await expression
    std::cout << "After co_await" << std::endl;
    co_return;
}

int main() {
    // Test member pointer type
    MyClass obj;
    obj.x = 42;
    obj.y = 3.14;
    
    // Access member through pointer
    std::cout << "obj.*intMemberPtr = " << obj.*intMemberPtr << std::endl;
    std::cout << "obj.*doubleMemberPtr = " << obj.*doubleMemberPtr << std::endl;
    
    // Call method through pointer
    (obj.*methodPtr)();
    std::cout << "obj.*getValuePtr() = " << (obj.*getValuePtr)() << std::endl;
    
    // Test co_await
    std::cout << "\nTesting co_await:" << std::endl;
    testCoroutine();
    
    return 0;
}
