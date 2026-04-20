int main() {
    int intValue = 10;
    double doubleValue = 3.14;
    
    // Test capture type inference (by copy)
    auto lambda = [intValue, doubleValue]() {
        return intValue + (int)doubleValue;
    };
    
    return 0;
}
