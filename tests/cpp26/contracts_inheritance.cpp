// Test: Contract inheritance (P2900R14) - P7.3.2
// End-to-end compile test for BlockType compiler

// Test case 1: Base class with pre contract, derived inherits it
class Base1 {
public:
    [[pre: x > 0]]
    virtual int foo(int x) { return x; }
};

class Derived1 : public Base1 {
public:
    int foo(int x) override { return x * 2; }
};

// Test case 2: Base with post contract, derived inherits it
class Base2 {
public:
    [[post: result > 0]]
    virtual int bar(int x) { return x + 1; }
};

class Derived2 : public Base2 {
public:
    int bar(int x) override { return x + 2; }
};

// Test case 3: Both base and derived have contracts
class Base3 {
public:
    [[pre: x >= 0]]
    virtual int baz(int x) { return x; }
};

class Derived3 : public Base3 {
public:
    [[post: result >= 0]]
    int baz(int x) override { return x * x; }
};

// Test case 4: Deep inheritance chain
class Base4 {
public:
    [[pre: x > 0]]
    [[post: result > 0]]
    virtual int qux(int x) { return x; }
};

class Middle4 : public Base4 {
public:
    int qux(int x) override { return x + 1; }
};

class Derived4 : public Middle4 {
public:
    int qux(int x) override { return x + 2; }
};

int main() {
    Derived1 d1;
    int r1 = d1.foo(5);

    Derived2 d2;
    int r2 = d2.bar(3);

    Derived3 d3;
    int r3 = d3.baz(4);

    Derived4 d4;
    int r4 = d4.qux(2);

    return r1 + r2 + r3 + r4 - 22;
}
