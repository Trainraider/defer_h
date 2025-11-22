#include <stdio.h>
#include <stdlib.h>
#define USE_C99_DEFER
#include "macro_stack.h"
#include "defer.h"

void cleanup_int(void* val) {
    int* int_val = (int*)val;
    printf("Cleaning up: %d\n", *int_val);
    *int_val = 0;
}

void cleanup_file(void* fp) {
    FILE** file_ptr = (FILE**)fp;
    if (*file_ptr) {
        printf("Closing file\n");
        fclose(*file_ptr);
        *file_ptr = NULL;
    }
}


int main() {
    S_
        printf("=== Basic Defer Demo ===\n\n");
    
        // Basic defer - cleanup runs in reverse order
        int a = 1;
        defer(cleanup_int, a);
    
        int b = 2;
        defer(cleanup_int, b);
    
        printf("Variables created: a=%d, b=%d\n", a, b);
    
        // Nested scope
        S_
            int c = 3;
            defer(cleanup_int, c);
            printf("In nested scope: c=%d\n", c);
            printf("Exiting nested scope...\n");
        _S

        printf("After nested scope\n");

        // errdefer only runs on returnerr
        // this won't run since this function returns normally
        int error = 0;
        errdefer(cleanup_int, error);
    
        // File example with cleanupdecl
        FILE* cleanupdecl(fp, fopen("/dev/null", "w"), cleanup_file);
        if (fp) {
            printf("File opened successfully\n");
        }
    
        printf("\nExiting main - watch LIFO cleanup order!\n");
        return 0;
    _S
    return 0;// Avoiding using special scopes for the outside of the function
    // and then having an unreachable extra return convinces the compiler the
    // function always returns a value, otherwise there's a warning.
}
