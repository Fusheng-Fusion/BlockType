// Test: if consteval (P1938R3)
// End-to-end compile test for BlockType compiler

// Test case 1: Basic if consteval
constexpr int test_basic_consteval() {
    if consteval {
        return 1;
    } else {
        return 0;
    }
}

// Test case 2: Negated if consteval
constexpr int test_negated_consteval() {
    if !consteval {
        return 0;
    } else {
        return 1;
    }
}

// Test case 3: if consteval in non-constexpr function
int test_consteval_in_regular() {
    if consteval {
        return 1;
    } else {
        return 0;
    }
}

// Test case 4: Nested if consteval
constexpr int test_nested_consteval() {
    if consteval {
        if consteval {
            return 2;
        }
        return 1;
    }
    return 0;
}

// Test case 5: if consteval with return value usage
constexpr int compute(int n) {
    if consteval {
        return n * 2;
    } else {
        return n;
    }
}

int test_consteval_return() {
    int r = compute(5);
    return r - 5;
}

int main() {
    int r1 = test_basic_consteval();
    int r2 = test_negated_consteval();
    int r3 = test_consteval_in_regular();
    int r4 = test_nested_consteval();
    int r5 = test_consteval_return();
    return r1 + r2 + r3 + r4 + r5;
}
