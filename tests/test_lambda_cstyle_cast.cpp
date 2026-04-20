int main() {
    double d = 3.14;
    
    auto lambda = [d]() -> int {
        return (int)d;
    };
    
    int result = lambda();
    return result;
}
