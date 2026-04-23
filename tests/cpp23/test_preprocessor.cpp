// Test: Preprocessor enhancements (#elifdef, #elifndef, #warning)
// P2334R1, P2437R1
// End-to-end compile test for BlockType compiler

// Test case 1: #elifdef basic
#define FEATURE_A
#ifdef FEATURE_B
int x1 = 1;
#elifdef FEATURE_A
int x1 = 2;
#else
int x1 = 3;
#endif

int test_elifdef() { return x1 - 2; }

// Test case 2: #elifndef basic
#define FEATURE_C
#ifdef FEATURE_C
int x2 = 10;
#elifndef FEATURE_D
int x2 = 20;
#else
int x2 = 30;
#endif

int test_elifndef() { return x2 - 10; }

// Test case 3: #elifndef with undefined macro
#ifdef UNDEFINED_MACRO
int x3 = 1;
#elifndef ALSO_UNDEFINED
int x3 = 99;
#else
int x3 = 0;
#endif

int test_elifndef_undefined() { return x3 - 99; }

// Test case 4: #warning directive
#warning "This is a test warning message"

int test_warning() { return 0; }

// Test case 5: Combined preprocessor directives
#define MODE 2
#if MODE == 1
int mode_val = 1;
#elifdef EXTRA_MODE
int mode_val = 2;
#else
int mode_val = 3;
#endif

int test_combined() { return mode_val - 3; }

int main() {
    int r1 = test_elifdef();
    int r2 = test_elifndef();
    int r3 = test_elifndef_undefined();
    int r4 = test_warning();
    int r5 = test_combined();
    return r1 + r2 + r3 + r4 + r5;
}
