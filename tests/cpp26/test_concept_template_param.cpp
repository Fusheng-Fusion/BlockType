// Test: Concept template template parameters (P2841R7)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic concept as template template parameter
template <typename T>
concept AlwaysTrue = true;

template <template <typename> concept C>
void constrained_func() {}

int test_basic_concept_ttp() {
    constrained_func<AlwaysTrue>();
    return 0;
}

// Test case 2: Concept template template param with requirement
template <typename T>
concept IsIntegral = requires { typename T; };

int test_concept_ttp_with_req() {
    return 0;
}

// Test case 3: Concept template template in class template
template <template <typename> concept C>
struct ConstrainedClass {};

int test_concept_ttp_class() {
    return 0;
}

int main() {
    int r1 = test_basic_concept_ttp();
    int r2 = test_concept_ttp_with_req();
    int r3 = test_concept_ttp_class();
    return r1 + r2 + r3;
}
