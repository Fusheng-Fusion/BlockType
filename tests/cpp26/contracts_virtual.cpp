// Test: Contract with virtual functions (P2900R14) - P7.3.2
// End-to-end compile test for BlockType compiler

// Test case 1: Virtual function with contract called through base pointer
class Shape {
public:
    [[pre: scale > 0]]
    virtual double area(double scale) { return 0.0; }
    virtual ~Shape() = default;
};

class Circle : public Shape {
public:
    double area(double scale) override { return 3.14 * scale; }
};

class Rectangle : public Shape {
public:
    double area(double scale) override { return 2.0 * scale; }
};

// Test case 2: Pure virtual function with contract
class Animal {
public:
    [[pre: energy > 0]]
    virtual void speak(int energy) = 0;
    virtual ~Animal() = default;
};

class Dog : public Animal {
public:
    void speak(int energy) override { (void)energy; }
};

// Test case 3: Virtual function with both pre and post
class Container {
public:
    [[pre: index >= 0]]
    [[post: result >= 0]]
    virtual int get(int index) { return 0; }
    virtual ~Container() = default;
};

class Array : public Container {
public:
    int get(int index) override { return index; }
};

int main() {
    Circle c;
    Rectangle r;
    Shape *s1 = &c;
    Shape *s2 = &r;
    double a1 = s1->area(1.0);
    double a2 = s2->area(2.0);

    Dog d;
    d.speak(10);

    Array arr;
    int v = arr.get(5);

    return static_cast<int>(a1 + a2) + v - 12;
}
