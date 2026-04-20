int main() {
    auto lambda = []() -> int {
        return 42;
    };
    
    int result = lambda();
    return result;
}
