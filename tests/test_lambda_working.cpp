int main() {
    int value = 42;
    
    // Test simple lambda with capture
    auto lambda = [value]() {
        return value;
    };
    
    return 0;
}
