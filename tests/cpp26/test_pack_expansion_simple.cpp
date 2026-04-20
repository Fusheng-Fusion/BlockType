// Simple test for P1061R10 pack expansion in structured binding
// This test uses a simple struct instead of std::tuple to avoid dependencies

struct MyTuple {
    int first;
    double second;
    char third;
};

void test_pack_expansion() {
    MyTuple t{42, 3.14, 'a'};
    
    // Pack expansion: ...rest should expand to rest0, rest1
    auto [first, ...rest] = t;
}

int main() {
    test_pack_expansion();
    return 0;
}
