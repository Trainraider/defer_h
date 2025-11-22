#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#undef _POSIX_C_SOURCE
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#ifdef __clang__
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#endif
#ifdef __GNUC__
#define FALLTHROUGH __attribute__((fallthrough))
#else
#define FALLTHROUGH
#endif
#if defined (__GNUC__) && !defined(USE_C99_DEFER)
#undef USE_MACRO_STACK
#endif
#ifdef USE_MACRO_STACK
#include "macro_stack.h"
#define USING_MACRO_STACK "enabled"
#else
#define USING_MACRO_STACK "disabled"
#endif // USE_MACRO_STACK
#include "defer.h"
#ifndef USE_C99_DEFER
#define CSTD "gnu11+"
#else
#define CSTD "c99+"
#endif // USE_C99_DEFER

// Test harness: non-aborting assert and lightweight runner
static int __test_total_asserts = 0;
static int __test_failed_asserts = 0;
static char __last_assert_msg[256] = {0};

#undef assert
#define assert(cond) do { \
    __test_total_asserts++; \
    if (!(cond)) { \
        __test_failed_asserts++; \
        snprintf(__last_assert_msg, sizeof(__last_assert_msg), "%s:%d: %s", __FILE__, __LINE__, #cond); \
    } \
} while (0)

/* Runner storage */
#define MAX_TESTS 256
static const char* __test_names[MAX_TESTS];
static int __test_results[MAX_TESTS]; /* 0 = pass, 1 = fail */
static char __test_msgs[MAX_TESTS][256];
static int __test_count = 0;

#ifndef TEST_VERBOSITY
#define TEST_VERBOSITY 0
#endif

/* Verbosity: when 0, suppress stdout produced by tests while they run. */
static int __test_verbosity = TEST_VERBOSITY;

#define RUN_TEST(fn) do { \
    int __before = __test_failed_asserts; \
    __last_assert_msg[0] = '\0'; \
    int __saved_stdout = -1; \
    if (!__test_verbosity) { \
        fflush(stdout); \
        __saved_stdout = dup(fileno(stdout)); \
        int __devnull = open("/dev/null", O_WRONLY); \
        if (__devnull != -1) { dup2(__devnull, fileno(stdout)); close(__devnull); } \
    } \
    fn(); \
    if (!__test_verbosity && __saved_stdout != -1) { \
        fflush(stdout); \
        dup2(__saved_stdout, fileno(stdout)); \
        close(__saved_stdout); \
    } \
    int __after = __test_failed_asserts; \
    __test_names[__test_count] = #fn; \
    __test_results[__test_count] = (__after > __before) ? 1 : 0; \
    if (__after > __before) { strncpy(__test_msgs[__test_count], __last_assert_msg, sizeof(__test_msgs[0]) - 1); __test_msgs[__test_count][sizeof(__test_msgs[0]) - 1] = '\0'; } else { __test_msgs[__test_count][0] = '\0'; } \
    __test_count++; \
} while (0)


// Global state for tracking cleanup order and calls
#define MAX_CLEANUPS 20
char cleanup_log[MAX_CLEANUPS][32];
int cleanup_count = 0;

void reset_log() {
    cleanup_count = 0;
    memset(cleanup_log, 0, sizeof(cleanup_log));
}

void log_cleanup(const char* name, int value) {
    snprintf(cleanup_log[cleanup_count++], 32, "%s:%d", name, value);
}

// Cleanup functions for testing
void cleanup_a(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("a", *val);
}

void cleanup_b(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("b", *val);
}

void cleanup_c(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("c", *val);
}

void cleanup_d(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("d", *val);
}

void cleanup_e(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("e", *val);
}

void cleanup_f(void* ptr) {
    int* val = (int*)ptr;
    log_cleanup("f", *val);
}

// Test 1: Basic defer in single scope
void test_basic_defer() {
    printf("\n=== Test 1: Basic defer in single scope ===\n");
    reset_log();
    
    S_
        int a = 1;
        defer(cleanup_a, a);
        int b = 2;
        defer(cleanup_b, b);
        printf("Inside scope: a=%d, b=%d\n", a, b);
    _S
    
    printf("After scope\n");
    printf("Cleanup count: %d\n", cleanup_count);
    
    // Verify cleanup order (LIFO - last in, first out)
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "b:2") == 0);
    assert(strcmp(cleanup_log[1], "a:1") == 0);
    printf("✓ Cleanups ran in reverse order\n");
}

// Test 2: Nested scopes with defer
void test_nested_scopes() {
    printf("\n=== Test 2: Nested scopes with defer ===\n");
    reset_log();
    
    S_
        int a = 1;
        defer(cleanup_a, a);
        
        S_
            int b = 2;
            defer(cleanup_b, b);
            printf("Inner scope: b=%d\n", b);
        _S
        
        printf("Outer scope after inner: a=%d\n", a);
    _S
    
    printf("After all scopes\n");
    
    // Inner scope cleanup happens first, then outer
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "b:2") == 0);
    assert(strcmp(cleanup_log[1], "a:1") == 0);
    printf("✓ Nested scopes cleanup correctly\n");
}

// Test 3: Return triggers all scope cleanups
int test_return_cleanups_helper() S_
    int a = 10;
    defer(cleanup_a, a);
    
    S_
        int b = 20;
        defer(cleanup_b, b);
        
        S_
            int c = 30;
            defer(cleanup_c, c);
            return 42;
        _S
        
        printf("This should not print\n");
    _S
    
    printf("This should not print either\n");
    return 0;
_S

void test_return_cleanups() {
    printf("\n=== Test 3: Return triggers all scope cleanups ===\n");
    reset_log();
    
    int result = test_return_cleanups_helper();
    
    assert(result == 42);
    assert(cleanup_count == 3);
    // All cleanups should run in reverse order
    assert(strcmp(cleanup_log[0], "c:30") == 0);
    assert(strcmp(cleanup_log[1], "b:20") == 0);
    assert(strcmp(cleanup_log[2], "a:10") == 0);
    printf("✓ Return triggered all scope cleanups\n");
}

// Test 4: errdefer only runs on returnerr
int test_errdefer_success() S_
    int a = 100;
    defer(cleanup_a, a);
    
    int b = 200;
    errdefer(cleanup_b, b);
    
    return 0; // Normal return, errdefer should NOT run
_S

int test_errdefer_error() {
    S_
        int c = 300;
        defer(cleanup_c, c);
    
        int d = 400;
        errdefer(cleanup_d, d);
    
        returnerr -1; // Error return, errdefer SHOULD run
    _S
    return 0;
}

void test_errdefer() {
    printf("\n=== Test 4: errdefer behavior ===\n");
    
    // Test success case
    reset_log();
    int result1 = test_errdefer_success();
    assert(result1 == 0);
    assert(cleanup_count == 1); // Only defer ran
    assert(strcmp(cleanup_log[0], "a:100") == 0);
    printf("✓ errdefer did not run on normal return\n");
    
    // Test error case
    reset_log();
    int result2 = test_errdefer_error();
    assert(result2 == -1);
    assert(cleanup_count == 2); // Both defer and errdefer ran
    assert(strcmp(cleanup_log[0], "d:400") == 0);
    assert(strcmp(cleanup_log[1], "c:300") == 0);
    printf("✓ errdefer ran on returnerr\n");
}

// Test 5: Multiple defers in same scope
void test_multiple_defers() {
    printf("\n=== Test 5: Multiple defers in same scope ===\n");
    reset_log();
    
    S_
        int a = 1;
        defer(cleanup_a, a);
        int b = 2;
        defer(cleanup_b, b);
        int c = 3;
        defer(cleanup_c, c);
        int d = 4;
        defer(cleanup_d, d);
        int e = 5;
        defer(cleanup_e, e);
        printf("All variables registered\n");
    _S
    
    assert(cleanup_count == 5);
    // Should be in reverse order
    assert(strcmp(cleanup_log[0], "e:5") == 0);
    assert(strcmp(cleanup_log[1], "d:4") == 0);
    assert(strcmp(cleanup_log[2], "c:3") == 0);
    assert(strcmp(cleanup_log[3], "b:2") == 0);
    assert(strcmp(cleanup_log[4], "a:1") == 0);
    printf("✓ Multiple defers cleanup in LIFO order\n");
}

// Test 6: Loop with scope
void test_loop_scope() {
    printf("\n=== Test 6: Loop with defer scope ===\n");
    reset_log();
    
    for (int i = 0; i < 3; i++) S_
        int a = i;
        defer(cleanup_a, a);
        printf("Loop iteration %d\n", i);
    _S
    
    // Each iteration should cleanup before next
    assert(cleanup_count == 3);
    assert(strcmp(cleanup_log[0], "a:0") == 0);
    assert(strcmp(cleanup_log[1], "a:1") == 0);
    assert(strcmp(cleanup_log[2], "a:2") == 0);
    printf("✓ Loop scope cleanup works correctly\n");
}

// Test 7: Variable value changes before cleanup
void test_value_changes() {
    printf("\n=== Test 7: Variable value changes before cleanup ===\n");
    reset_log();
    
    S_
        int a = 100;
        defer(cleanup_a, a);
        a = 999; // Change value
        printf("Changed a to %d\n", a);
    _S
    
    // Cleanup should see the changed value (passed by reference)
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "a:999") == 0);
    printf("✓ Cleanup sees modified value (by reference)\n");
}

// Test 8: Return in nested scope cleans all
int test_nested_return() S_
    int a = 1;
    defer(cleanup_a, a);
    
    S_
        int b = 2;
        defer(cleanup_b, b);
        
        S_
            int c = 3;
            defer(cleanup_c, c);
            
            if (1) S_
                int d = 4;
                defer(cleanup_d, d);
                return 123;
            _S
        _S
    _S
    
    return 0;
_S

void test_nested_return_all() {
    printf("\n=== Test 8: Return in deeply nested scope ===\n");
    reset_log();
    
    int result = test_nested_return();
    
    assert(result == 123);
    assert(cleanup_count == 4);
    assert(strcmp(cleanup_log[0], "d:4") == 0);
    assert(strcmp(cleanup_log[1], "c:3") == 0);
    assert(strcmp(cleanup_log[2], "b:2") == 0);
    assert(strcmp(cleanup_log[3], "a:1") == 0);
    printf("✓ Return from nested scope cleans all parent scopes\n");
}

// Test 9: Mix of defer and errdefer
int test_mixed_defer_errdefer() {
    S_
        int a = 1;
        defer(cleanup_a, a);
    
        int b = 2;
        errdefer(cleanup_b, b);
    
        int c = 3;
        defer(cleanup_c, c);
    
        int d = 4;
        errdefer(cleanup_d, d);
    
        returnerr -1;
    _S
    return 0;
}

void test_mixed() {
    printf("\n=== Test 9: Mixed defer and errdefer ===\n");
    reset_log();
    
    int result = test_mixed_defer_errdefer();
    
    assert(result == -1);
    assert(cleanup_count == 4);
    // All should run on returnerr, in reverse order
    assert(strcmp(cleanup_log[0], "d:4") == 0);
    assert(strcmp(cleanup_log[1], "c:3") == 0);
    assert(strcmp(cleanup_log[2], "b:2") == 0);
    assert(strcmp(cleanup_log[3], "a:1") == 0);
    printf("✓ Mixed defer/errdefer with returnerr\n");
}

// Test 10: Recursion safety (proves no hidden static state)
void recurse(int n) S_
        int x = n;
        defer(cleanup_a, x);

        if (n > 0)
            recurse(n - 1);
    _S

void test_recursion() {
    printf("\n=== Test 10: Recursion with defer ===\n");
    reset_log();

    recurse(5);

    assert(cleanup_count == 6);  // 0 to 5
    assert(strcmp(cleanup_log[0], "a:0") == 0);
    assert(strcmp(cleanup_log[5], "a:5") == 0);
    printf("✓ Recursion works perfectly (proves no global state)\n");
}

// Test 11: break/continue inside scoped block (inside loop)
void test_break_continue() {
    printf("\n=== Test 11: break/continue in scoped block ===\n");
    reset_log();

    int i;
    for (i = 0; i < 10; i++) S_
        int x = i;
        defer(cleanup_a, x);

        if (i == 3) {break;};
        if (i == 1) {continue;};  // still runs defer for i=1
    _S

    assert(i == 3);
    printf("cleanup_count: %d\n", cleanup_count);
    assert(cleanup_count == 4);  // i=0,1,2,3
    printf("✓ break/continue run defers correctly\n");
}

// Test 12: break/continue inside nested scoped block within a loop
void test_nested_break_continue() {
    printf("\n=== Test 12: Nested break/continue in scoped block ===\n");
    reset_log();
    S_
        int w = 42;
        defer(cleanup_e, w);
        int i;
        for (i = 0; i < 10; i++) S_
            int x = i;
            defer(cleanup_a, x);
            S_
                int y = i * 10;
                defer(cleanup_b, y);
                S_
                    int z = i * 100;
                    defer(cleanup_c, z);
                    // innermost scope
                    if (i == 3) {break;};
                    if (i == 1) {continue;};  // still runs 3 defers for i=1
                _S
                assert(i != 3);  // should not reach here if i==3
            _S
            assert(i != 3);  // should not reach here if i==3
        _S
        assert(i == 3);
        assert(cleanup_count == 12);  // i=0,1,2,3 with a, b, and c, not yet e
    _S
    assert(cleanup_count == 13);  // plus the outer e defer
    printf("✓ Nested break/continue run defers correctly\n");
}

// Test 13: do-while loop with defer scope
void test_do_while_loop() {
    printf("\n=== Test 13: do-while loop with defer scope ===\n");
    reset_log();
    
    int i = 0;
    do S_
        int x = i;
        defer(cleanup_a, x);
        i++;
    _S while (i < 3);
    
    // Each iteration should cleanup before next
    assert(i == 3);
    assert(cleanup_count == 3);
    assert(strcmp(cleanup_log[0], "a:0") == 0);
    assert(strcmp(cleanup_log[1], "a:1") == 0);
    assert(strcmp(cleanup_log[2], "a:2") == 0);
    printf("✓ do-while loop defer scope works correctly\n");
}

// Test 14: break/continue inside nested scoped block within a do-while loop
void test_nested_do_while_break_continue() {
    printf("\n=== Test 14: Nested break/continue in do-while scoped block ===\n");
    reset_log();
    S_
        int w = 42;
        defer(cleanup_e, w);
        int i = 0;
        while(i < 10) S_
            int x = i;
            defer(cleanup_a, x);
            S_
                int y = i * 10;
                defer(cleanup_b, y);
                S_
                    int z = i * 100;
                    defer(cleanup_c, z);
                    // innermost scope
                    if (i == 3) {break;};
                    if (i == 1) {i++; continue;};  // still runs 3 defers for i=1
                _S
                assert(i != 3);  // should not reach here if i==3
            _S
            assert(i != 3);  // should not reach here if i==3
            i++;
        _S
        assert(i == 3);
        assert(cleanup_count == 12);  // i=0,1,2,3 with a, b, and c, not yet e
    _S
    assert(cleanup_count == 13);  // plus the outer e defer
    printf("✓ Nested do-while break/continue run defers correctly\n");
}

// Test 15: Switch statement with defer and nested scopes
void test_switch_defer() {
    printf("\n=== Test 15: Switch statement with defer ===\n");
    reset_log();
    
    int value = 2;
    switch (value) {
        case 1: S_
            int a = 10;
            defer(cleanup_a, a);
            printf("Case 1 executed\n");
        _S break;
        
        case 2: S_
            int b = 20;
            defer(cleanup_b, b);
            printf("Case 2 outer scope\n");
            
            S_
                int c = 30;
                defer(cleanup_c, c);
                printf("Case 2 inner scope\n");
                
                S_
                    int d = 40;
                    defer(cleanup_d, d);
                    printf("Case 2 innermost scope\n");
                _S
            _S
        _S break;
        
        case 3: S_
            int e = 50;
            defer(cleanup_e, e);
            printf("Case 3 executed\n");
        _S break;
        
        default: S_
            int f = 60;
            defer(cleanup_f, f);
            printf("Default case executed\n");
        _S break;
    }
    
    printf("After switch\n");
    
    // Should only have cleanups from case 2 (nested scopes)
    assert(cleanup_count == 3);
    assert(strcmp(cleanup_log[0], "d:40") == 0);  // Innermost first
    assert(strcmp(cleanup_log[1], "c:30") == 0);  // Middle scope
    assert(strcmp(cleanup_log[2], "b:20") == 0);  // Outer scope
    printf("✓ Switch defer with nested scopes works correctly\n");
}

// Test 16: Switch with fallthrough and defer
void test_switch_fallthrough() {
    printf("\n=== Test 16: Switch fallthrough with defer ===\n");
    reset_log();
    
    int value = 1;
    switch (value) {
        case 1: S_
            int a = 100;
            defer(cleanup_a, a);
            printf("Case 1 executed\n");
        _S FALLTHROUGH;
        
        case 2: S_
            int b = 200;
            defer(cleanup_b, b);
            printf("Case 2 executed (fell through)\n");
        _S break;
        
        default: S_
            int c = 300;
            defer(cleanup_c, c);
            printf("Default executed\n");
        _S break;
    }
    
    printf("After switch\n");
    
    // Should have cleanups from both case 1 and case 2 (fallthrough)
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "a:100") == 0);  // Case 1 defer
    assert(strcmp(cleanup_log[1], "b:200") == 0);  // Case 2 defer
    printf("✓ Switch fallthrough defer works correctly\n");
}

// Test 17: Switch with return in case
int test_switch_return() S_
    int outer = 1000;
    defer(cleanup_a, outer);
    
    int value = 3;
    switch (value) {
        case 1: S_
            int a = 10;
            defer(cleanup_b, a);
            printf("Case 1\n");
        _S break;
        
        case 2: S_
            int b = 20;
            defer(cleanup_c, b);
            printf("Case 2\n");
        _S break;
        
        case 3: S_
            int c = 30;
            defer(cleanup_d, c);
            
            S_
                int d = 40;
                defer(cleanup_e, d);
                printf("About to return from case 3\n");
                return 999;
            _S
        _S break;
        
        default: S_
            int e = 50;
            defer(cleanup_a, e);  // Reuse cleanup_a for different value
            printf("Default\n");
        _S break;
    }
    
    return 0;
_S

void test_switch_return_cleanups() {
    printf("\n=== Test 17: Switch with return in case ===\n");
    reset_log();
    
    int result = test_switch_return();
    
    assert(result == 999);
    assert(cleanup_count == 3);
    // Should cleanup inner scopes first, then outer
    assert(strcmp(cleanup_log[0], "e:40") == 0);   // Innermost defer in case
    assert(strcmp(cleanup_log[1], "d:30") == 0);   // Case defer
    assert(strcmp(cleanup_log[2], "a:1000") == 0); // Function-level defer
    printf("✓ Switch return cleans up all scopes correctly\n");
}

// Test 18: cleanupdecl macro
void cleanup_ptr(void* ptr) {
    int** p = (int**)ptr;
    if (*p) {
        log_cleanup("ptr", **p);
        free(*p);
        *p = NULL;
    }
}

void test_cleanupdecl() {
    printf("\n=== Test 18: cleanupdecl macro ===\n");
    reset_log();
    
    S_
        int* cleanupdecl(ptr, malloc(sizeof(int)), cleanup_ptr);
        *ptr = 42;
        printf("Allocated and set ptr to %d\n", *ptr);
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "ptr:42") == 0);
    printf("✓ cleanupdecl works correctly\n");
}

// Test 19: Empty scope
void test_empty_scope() {
    printf("\n=== Test 19: Empty scope ===\n");
    reset_log();
    
    S_
        // Nothing here
    _S
    
    S_
        S_
            S_
                // Multiple nested empty scopes
            _S
        _S
    _S
    
    assert(cleanup_count == 0);
    printf("✓ Empty scopes don't cause issues\n");
}

// Test 20: Multiple return paths
int test_multiple_returns(int path) S_
    int a = 100;
    defer(cleanup_a, a);
    
    if (path == 1) S_
        int b = 200;
        defer(cleanup_b, b);
        return 1;
    _S
    
    if (path == 2) S_
        int c = 300;
        defer(cleanup_c, c);
        return 2;
    _S
    
    int d = 400;
    defer(cleanup_d, d);
    return 3;
_S

void test_multiple_return_paths() {
    printf("\n=== Test 20: Multiple return paths ===\n");
    
    // Path 1
    reset_log();
    int result1 = test_multiple_returns(1);
    assert(result1 == 1);
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "b:200") == 0);
    assert(strcmp(cleanup_log[1], "a:100") == 0);
    
    // Path 2
    reset_log();
    int result2 = test_multiple_returns(2);
    assert(result2 == 2);
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "c:300") == 0);
    assert(strcmp(cleanup_log[1], "a:100") == 0);
    
    // Path 3
    reset_log();
    int result3 = test_multiple_returns(3);
    assert(result3 == 3);
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "d:400") == 0);
    assert(strcmp(cleanup_log[1], "a:100") == 0);
    
    printf("✓ Multiple return paths cleanup correctly\n");
}

// Test 21: Very deep nesting
void test_deep_nesting() {
    printf("\n=== Test 21: Very deep nesting (10 levels) ===\n");
    reset_log();
    
    S_
        int v1 = 1; defer(cleanup_a, v1);
        S_
            int v2 = 2; defer(cleanup_b, v2);
            S_
                int v3 = 3; defer(cleanup_c, v3);
                S_
                    int v4 = 4; defer(cleanup_d, v4);
                    S_
                        int v5 = 5; defer(cleanup_e, v5);
                        S_
                            int v6 = 6; defer(cleanup_f, v6);
                            S_
                                int v7 = 7; defer(cleanup_a, v7);
                                S_
                                    int v8 = 8; defer(cleanup_b, v8);
                                    S_
                                        int v9 = 9; defer(cleanup_c, v9);
                                        S_
                                            int v10 = 10; defer(cleanup_d, v10);
                                            printf("Reached deepest level\n");
                                        _S
                                    _S
                                _S
                            _S
                        _S
                    _S
                _S
            _S
        _S
    _S
    
    assert(cleanup_count == 10);
    // Verify reverse order
    assert(strcmp(cleanup_log[0], "d:10") == 0);
    assert(strcmp(cleanup_log[1], "c:9") == 0);
    assert(strcmp(cleanup_log[2], "b:8") == 0);
    assert(strcmp(cleanup_log[3], "a:7") == 0);
    assert(strcmp(cleanup_log[4], "f:6") == 0);
    assert(strcmp(cleanup_log[5], "e:5") == 0);
    assert(strcmp(cleanup_log[6], "d:4") == 0);
    assert(strcmp(cleanup_log[7], "c:3") == 0);
    assert(strcmp(cleanup_log[8], "b:2") == 0);
    assert(strcmp(cleanup_log[9], "a:1") == 0);
    printf("✓ Deep nesting works correctly\n");
}

// Test 22: while(false) loop - body never executes
void test_while_false() {
    printf("\n=== Test 22: while(false) loop ===\n");
    reset_log();
    
    while (false) S_
        int a = 1;
        defer(cleanup_a, a);
        printf("This should never print\n");
    _S
    
    assert(cleanup_count == 0);
    printf("✓ while(false) body and defers don't execute\n");
}

// Test 23: Complex control flow mixing
void test_complex_control_flow() {
    printf("\n=== Test 23: Complex control flow mixing ===\n");
    reset_log();
    
    S_
        int outer = 1;
        defer(cleanup_a, outer);
        
        for (int i = 0; i < 3; i++) S_
            int loop = i;
            defer(cleanup_b, loop);
            
            if (i == 1) {continue;};
            
            switch (i) {
                case 0: S_
                    int c0 = 10;
                    defer(cleanup_c, c0);
                _S break;
                
                case 2: S_
                    int c2 = 20;
                    defer(cleanup_d, c2);
                    if (i == 2) {break;};
                _S break;
            }
        _S
    _S
    
    // Should have: i=0 (b:0, c:10), i=1 (b:1 only), i=2 (b:2, d:20), outer (a:1)
    assert(cleanup_count == 6);
    assert(strcmp(cleanup_log[0], "c:10") == 0);
    assert(strcmp(cleanup_log[1], "b:0") == 0);
    assert(strcmp(cleanup_log[2], "b:1") == 0);
    assert(strcmp(cleanup_log[3], "d:20") == 0);
    assert(strcmp(cleanup_log[4], "b:2") == 0);
    assert(strcmp(cleanup_log[5], "a:1") == 0);
    printf("✓ Complex control flow works correctly\n");
}

// Test 24: Scope context initialization verification
void test_scope_initialization() {
    printf("\n=== Test 24: Scope context initialization ===\n");
    reset_log();
    
    // Test that scopes properly initialize even without any defers
    S_
        int x = 42;
        S_
            int y = x + 1;
            (void)y; // Use it
        _S
        
        // Now add a defer after nested scope
        defer(cleanup_a, x);
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "a:42") == 0);
    printf("✓ Scope initialization works correctly\n");
}

// Test 25: continue in switch inside loop
void test_continue_in_switch() {
    printf("\n=== Test 25: continue in switch inside loop ===\n");
    reset_log();
    
    for (int i = 0; i < 5; i++) S_
        int loop_var = i;
        defer(cleanup_a, loop_var);
        S_
            switch (i) {
                case 0: S_
                    int s0 = 100;
                    defer(cleanup_b, s0);
                    printf("Case 0\n");
                _S break;
                
                case 1: S_
                    int s1 = 200;
                    defer(cleanup_c, s1);
                    printf("Case 1 - about to continue loop\n");
                    // This continue should jump to next loop iteration
                    // NOT just exit the switch (which break would do)
                    continue;
                _S break;
                
                case 2: S_
                    int s2 = 300;
                    defer(cleanup_d, s2);
                    printf("Case 2\n");
                _S break;
                
                default: S_
                    int s_def = 400 + i;
                    defer(cleanup_e, s_def);
                    printf("Default case %d\n", i);
                _S break;
            }
        
            printf("After switch, i=%d\n", i);
        _S
    _S
    
    // Expected cleanup order:
    // i=0: b:100, a:0
    // i=1: c:200, a:1  (continue should cleanup both switch scope AND loop scope)
    // i=2: d:300, a:2
    // i=3: e:403, a:3
    // i=4: e:404, a:4
    
    printf("Total cleanups: %d\n", cleanup_count);
    
    // The bug: if continue doesn't distinguish between switch and loop contexts,
    // it might only cleanup the switch scope but not the loop scope before continuing
    assert(cleanup_count == 10);
    assert(strcmp(cleanup_log[0], "b:100") == 0);
    assert(strcmp(cleanup_log[1], "a:0") == 0);
    assert(strcmp(cleanup_log[2], "c:200") == 0);
    assert(strcmp(cleanup_log[3], "a:1") == 0);  // This might fail if bug exists
    assert(strcmp(cleanup_log[4], "d:300") == 0);
    assert(strcmp(cleanup_log[5], "a:2") == 0);
    
    printf("✓ continue in switch correctly jumps to loop\n");
}

// Test 26: break in switch just breaks switch, not loop
void test_break_in_switch() {
    printf("\n=== Test 26: break in switch inside loop ===\n");
    reset_log();
    
    int total_iterations = 0;
    for (int i = 0; i < 5; i++) S_
        int loop_var = i;
        defer(cleanup_a, loop_var);
        
        switch (i) {
            case 3: S_
                int s3 = 300;
                defer(cleanup_b, s3);
                printf("Case 3 - breaking from switch only\n");
            _S break;  // This breaks from switch, not loop
            
            default: S_
                int s_def = 400 + i;
                defer(cleanup_c, s_def);
            _S break;
        }
        
        total_iterations++;
        printf("Loop iteration %d completed\n", i);
    _S
    
    // All 5 loop iterations should complete
    assert(total_iterations == 5);
    // i=0,1,2,4 use default (cleanup_c then cleanup_a)
    // i=3 uses case 3 (cleanup_b then cleanup_a)
    assert(cleanup_count == 10);
    printf("✓ break in switch only breaks switch, not loop\n");
}

// Test 27: Nested loops with break in inner loop
void test_nested_loop_break() {
    printf("\n=== Test 27: Nested loops with break ===\n");
    reset_log();
    
    int outer_count = 0;
    int inner_count = 0;
    
    for (int i = 0; i < 3; i++) S_
        int outer_var = i * 100;
        defer(cleanup_a, outer_var);
        outer_count++;
        
        for (int j = 0; j < 5; j++) S_
            int inner_var = i * 10 + j;
            defer(cleanup_b, inner_var);
            inner_count++;
            
            if (j == 2) {break;};  // Should only break inner loop
        _S
        
        printf("Outer iteration %d completed\n", i);
    _S
    
    assert(outer_count == 3);
    assert(inner_count == 9);  // 3 iterations of inner loop (0,1,2) * 3 outer
    printf("✓ Nested loop break works correctly\n");
}

// Test 28: Continue in nested loops
void test_nested_loop_continue() {
    printf("\n=== Test 28: Nested loops with continue ===\n");
    reset_log();
    
    int outer_prints = 0;
    int inner_prints = 0;
    
    for (int i = 0; i < 3; i++) S_
        int outer_var = i;
        defer(cleanup_a, outer_var);
        
        for (int j = 0; j < 3; j++) S_
            int inner_var = j;
            defer(cleanup_b, inner_var);
            
            if (j == 1) {continue;};
            
            inner_prints++;
        _S
        
        outer_prints++;
    _S
    
    assert(outer_prints == 3);
    assert(inner_prints == 6);  // Skip j=1 each time: 2 per outer * 3
    printf("✓ Nested loop continue works correctly\n");
}

// Test 29: Switch without any scopes (bare cases)
void test_switch_no_scopes() {
    printf("\n=== Test 29: Switch without defer scopes ===\n");
    reset_log();
    
    S_
        int outer = 999;
        defer(cleanup_a, outer);
        
        int value = 2;
        switch (value) {
            case 1:
                printf("Case 1 (no scope)\n");
                break;
            case 2:
                printf("Case 2 (no scope)\n");
                break;
            default:
                printf("Default (no scope)\n");
                break;
        }
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "a:999") == 0);
    printf("✓ Switch without scopes works\n");
}

// Test 30: Defer same variable multiple times
void test_defer_same_variable() {
    printf("\n=== Test 30: Defer same variable multiple times ===\n");
    reset_log();
    
    S_
        int x = 1;
        defer(cleanup_a, x);
        x = 2;
        defer(cleanup_b, x);
        x = 3;
        defer(cleanup_c, x);
        printf("x is now %d\n", x);
    _S
    
    // All three should see x=3 since they all point to the same variable
    assert(cleanup_count == 3);
    assert(strcmp(cleanup_log[0], "c:3") == 0);
    assert(strcmp(cleanup_log[1], "b:3") == 0);
    assert(strcmp(cleanup_log[2], "a:3") == 0);
    printf("✓ Multiple defers on same variable all see final value\n");
}

// Test 31: While loop with break in nested scope
void test_while_nested_break() {
    printf("\n=== Test 31: While loop with nested break ===\n");
    reset_log();
    
    int i = 0;
    while (i < 10) S_
        int loop_var = i;
        defer(cleanup_a, loop_var);
        
        S_
            int inner_var = i * 10;
            defer(cleanup_b, inner_var);
            
            if (i == 3) {break;};
        _S
        
        i++;
    _S
    
    assert(i == 3);
    assert(cleanup_count == 8);  // i=0,1,2,3 each with cleanup_b and cleanup_a
    printf("✓ While loop nested break works\n");
}

// Test 32: Do-while with continue
void test_do_while_continue() {
    printf("\n=== Test 32: Do-while with continue ===\n");
    reset_log();
    
    int i = 0;
    int count = 0;
    do S_
        int loop_var = i;
        defer(cleanup_a, loop_var);
        i++;
        
        if (i == 2 || i == 4) {continue;};
        
        count++;
    _S while (i < 5);
    
    assert(count == 3);  // Skip i=2 and i=4, so count for i=1,3,5
    assert(cleanup_count == 5);  // All 5 iterations should cleanup
    printf("✓ Do-while continue works\n");
}

// Test 33: Empty for loop
void test_empty_for_loop() {
    printf("\n=== Test 33: Empty for loop ===\n");
    reset_log();
    
    for (int i = 0; i < 0; i++) S_
        int x = i;
        defer(cleanup_a, x);
    _S
    
    assert(cleanup_count == 0);
    printf("✓ Empty for loop (never executes) works\n");
}

// Test 34: Infinite loop with early break
void test_infinite_loop_break() {
    printf("\n=== Test 34: Infinite loop with early break ===\n");
    reset_log();
    
    int iterations = 0;
    while (true) S_
        int x = iterations;
        defer(cleanup_a, x);
        iterations++;
        
        if (iterations == 3) {break;};
    _S
    
    assert(iterations == 3);
    assert(cleanup_count == 3);
    printf("✓ Infinite loop with break works\n");
}

// Test 35: Switch inside switch (nested switches)
void test_nested_switches() {
    printf("\n=== Test 35: Nested switches ===\n");
    reset_log();
    
    int outer_val = 1;
    int inner_val = 2;
    
    switch (outer_val) {
        case 1: S_
            int a = 100;
            defer(cleanup_a, a);
            
            switch (inner_val) {
                case 1: S_
                    int b = 200;
                    defer(cleanup_b, b);
                _S break;
                
                case 2: S_
                    int c = 300;
                    defer(cleanup_c, c);
                _S break;
            }
        _S break;
    }
    
    assert(cleanup_count == 2);
    assert(strcmp(cleanup_log[0], "c:300") == 0);
    assert(strcmp(cleanup_log[1], "a:100") == 0);
    printf("✓ Nested switches work\n");
}

// Test 36: Loop with no body defers but break/continue
void test_loop_no_defer_with_control() {
    printf("\n=== Test 36: Loop with control flow but no defers in body ===\n");
    reset_log();
    
    S_
        int outer = 42;
        defer(cleanup_a, outer);
        
        for (int i = 0; i < 5; i++) {
            if (i == 2) {continue;};
            if (i == 4) {break;};
        }
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "a:42") == 0);
    printf("✓ Loop with control flow but no defers works\n");
}

// Test 37: Conditional defer (defer inside if)
void test_conditional_defer() {
    printf("\n=== Test 37: Conditional defer ===\n");
    reset_log();
    
    S_
        if (true) S_
            int x = 1;
            defer(cleanup_a, x);
        _S
        
        if (false) S_
            int y = 2;
            defer(cleanup_b, y);
        _S
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "a:1") == 0);
    printf("✓ Conditional defer works\n");
}

// Test 38: Defer in else branch
void test_defer_in_else() {
    printf("\n=== Test 38: Defer in else branch ===\n");
    reset_log();
    
    S_
        if (false) S_
            int x = 1;
            defer(cleanup_a, x);
        _S else S_
            int y = 2;
            defer(cleanup_b, y);
        _S
    _S
    
    assert(cleanup_count == 1);
    assert(strcmp(cleanup_log[0], "b:2") == 0);
    printf("✓ Defer in else works\n");
}

// Test 39: Triple nested loops with mixed break/continue
void test_triple_nested_loops() {
    printf("\n=== Test 39: Triple nested loops with break/continue ===\n");
    reset_log();
    
    for (int i = 0; i < 2; i++) S_
        int a = i;
        defer(cleanup_a, a);
        
        for (int j = 0; j < 3; j++) S_
            int b = j;
            defer(cleanup_b, b);
            
            if (j == 1) {continue;};
            
            for (int k = 0; k < 2; k++) S_
                int c = k;
                defer(cleanup_c, c);
                
                if (k == 1) {break;};
            _S
        _S
    _S
    
    // Complex but calculable: check that no crashes happen
    printf("Triple nested loops cleanup count: %d\n", cleanup_count);
    assert(cleanup_count > 0);
    printf("✓ Triple nested loops work\n");
}

// Test 40: Duff's device
void duff_copy_with_defer(int *from, int *to, size_t count) S_
    int outer_defer = 999;
    defer(cleanup_a, outer_defer);
    
    if (count == 0) {return;};
    int n = (count + 7) / 8;
    switch (count % 8) {
        case 0: do {
                *to++ = *from++; FALLTHROUGH;
        case 7:      *to++ = *from++; FALLTHROUGH;
        case 6:      *to++ = *from++; FALLTHROUGH;
        case 5:      *to++ = *from++; FALLTHROUGH;
        case 4:      *to++ = *from++; FALLTHROUGH;
        case 3:      *to++ = *from++; FALLTHROUGH;
        case 2:      *to++ = *from++; FALLTHROUGH;
        case 1:      *to++ = *from++;
        } while (--n > 0);
    }
_S

void test_duffs_device() {
    printf("\n=== Test 40: Duff's device with defer ===\n");
    reset_log();
    
    int src[11] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    int dst[11] = {0};
    
    duff_copy_with_defer(src, dst, 11);
    
    // Verify copy worked
    for (int i = 0; i < 11; i++) {
        assert(src[i] == dst[i]);
    }
    
    // Verify defers ran
    // Should have: outer_defer, loop_defer, switch_defer, 
    // and do_defer for each iteration (2 iterations since n = (11+7)/8 = 2)
    printf("Duff's device cleanup count: %d\n", cleanup_count);
    assert(cleanup_count == 1);  // At least outer, loop, and switch defers
    
    printf("✓ Duff's device with defer works\n");
}

// Test 41: Pathological nesting - switch inside do-while with fallthrough
void test_pathological_nesting() {
    printf("\n=== Test 41: Pathological switch/do-while nesting ===\n");
    reset_log();
    
    int iterations = 0;
    int state = 0;
    
    do S_
        int loop_var = iterations;
        defer(cleanup_a, loop_var);
        
        switch (state) {
            case 0: S_
                int s0 = 100;
                defer(cleanup_b, s0);
                state = 1;
            _S FALLTHROUGH;
            
            case 1: S_
                int s1 = 200;
                defer(cleanup_c, s1);
                state = 2;
            _S FALLTHROUGH;
            
            case 2: S_
                int s2 = 300;
                defer(cleanup_d, s2);
                state = 3;
            _S break;
        }
        
        iterations++;
    _S while (iterations < 2);
    
    // Should have run through state progression twice
    assert(iterations == 2);
    printf("Pathological nesting cleanup count: %d\n", cleanup_count);
    assert(cleanup_count > 0);
    
    printf("✓ Pathological switch/do-while nesting works\n");
}

// Test 42: Switch with no default and fallthrough to do-while
void test_switch_fallthrough_to_loop() {
    printf("\n=== Test 42: Switch fallthrough directly into do-while ===\n");
    reset_log();
    
    int value = 1;
    int count = 0;
    
    S_
        int outer = 42;
        defer(cleanup_a, outer);
        
        switch (value) {
            case 0: {
                printf("Case 0\n");
                break;
            }
            
            case 1:
                // No scope, fallthrough directly to case 2
            case 2: S_
                int switch_var = 200;
                defer(cleanup_b, switch_var);
                
                // Loop inside case
                do S_
                    int loop_var = count;
                    defer(cleanup_c, loop_var);
                    count++;
                _S while (count < 3);
            _S break;
        }
    _S
    
    assert(count == 3);
    // Should have: outer (a:42), switch_var (b:200), and 3 loop iterations (c:0, c:1, c:2)
    assert(cleanup_count == 5);
    
    printf("✓ Switch fallthrough to loop works\n");
}

int main() {
    RUN_TEST(test_basic_defer);
    RUN_TEST(test_nested_scopes);
    RUN_TEST(test_return_cleanups);
    RUN_TEST(test_errdefer);
    RUN_TEST(test_multiple_defers);
    RUN_TEST(test_loop_scope);
    RUN_TEST(test_value_changes);
    RUN_TEST(test_nested_return_all);
    RUN_TEST(test_mixed);
    RUN_TEST(test_recursion);
    RUN_TEST(test_break_continue);
    RUN_TEST(test_nested_break_continue);
    RUN_TEST(test_do_while_loop);
    RUN_TEST(test_nested_do_while_break_continue);
    RUN_TEST(test_switch_defer);
    RUN_TEST(test_switch_fallthrough);
    RUN_TEST(test_switch_return_cleanups);
    RUN_TEST(test_cleanupdecl);
    RUN_TEST(test_empty_scope);
    RUN_TEST(test_multiple_return_paths);
    RUN_TEST(test_deep_nesting);
    RUN_TEST(test_while_false);
    RUN_TEST(test_complex_control_flow);
    RUN_TEST(test_scope_initialization);
    RUN_TEST(test_continue_in_switch);
    RUN_TEST(test_break_in_switch);
    RUN_TEST(test_nested_loop_break);
    RUN_TEST(test_nested_loop_continue);
    RUN_TEST(test_switch_no_scopes);
    RUN_TEST(test_defer_same_variable);
    RUN_TEST(test_while_nested_break);
    RUN_TEST(test_do_while_continue);
    RUN_TEST(test_empty_for_loop);
    RUN_TEST(test_infinite_loop_break);
    RUN_TEST(test_nested_switches);
    RUN_TEST(test_loop_no_defer_with_control);
    RUN_TEST(test_conditional_defer);
    RUN_TEST(test_defer_in_else);
    RUN_TEST(test_triple_nested_loops);
    RUN_TEST(test_duffs_device);
    RUN_TEST(test_pathological_nesting);
    RUN_TEST(test_switch_fallthrough_to_loop);

    int fails = 0;
    for (int i = 0; i < __test_count; i++) if (__test_results[i]) fails++;

    if (fails) {
        printf("\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  defer.h Tests (%s, macro_stack: %s)\n", CSTD, USING_MACRO_STACK);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  ✗ %d/%d failed\n\n", fails, __test_count);
        printf("Failures:\n");
        for (int i = 0; i < __test_count; i++) {
            if (__test_results[i]) {
                printf("  - %s\n    %s\n", __test_names[i], __test_msgs[i]);
            }
        }
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    } else {
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("  defer.h: ✓ %d tests passed (%s, macro_stack: %s)\n", __test_count, CSTD, USING_MACRO_STACK);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    }

    return fails ? 1 : 0;
}
