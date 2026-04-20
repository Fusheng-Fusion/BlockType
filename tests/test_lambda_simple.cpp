int main() {
    auto lambda = []() {
        return 42;
    };
    
    int result = lambda();
    return result;
}
