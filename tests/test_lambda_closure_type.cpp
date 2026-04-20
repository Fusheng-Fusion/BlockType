int main() {
    int x = 42;
    
    auto lambda = [x]() -> int {
        return x;
    };
    
    return 0;
}
