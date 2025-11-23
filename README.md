# defer.h

A Zig-style `defer` and `errdefer` implementation for C, bringing reliable resource cleanup and error handling to C99+ and GNU C.

## Features

- **`defer`**: Schedule cleanup code to run when scope exits (LIFO order)
- **`errdefer`**: Conditional cleanup that only runs on error paths
- **Scope-based**: Works with `S_` `_S` scope delimiters
- **Control flow aware**: Properly handles `return`, `break`, `continue` in loops and switches
- **Two implementations**: 
  - GNU C: Uses `__attribute__((cleanup))` for minimal overhead
  - C99: Uses a stack allocated linked list to track deferred operations.
- **Zero global state**: Fully reentrant and thread-safe (There's a couple always-null mutable globals to bootstrap modified keywords; not stateful)
- **Comprehensive**: Handles nested scopes, loops, switches, and complex control flow

## Quick Start

```c
#include "defer.h"

void example() S_
    FILE* f = fopen("data.txt", "r");
    defer(fclose_wrapper, f);  // Always closes file on scope exit
    
    char* buffer = malloc(1024);
    defer(free_wrapper, buffer);  // Always frees buffer
    
    // Your code here - resources cleaned up automatically
    if (error_condition) {
        returnerr;  // defer runs: buffer freed, file closed
    }
    
    // Normal exit - defer still runs: buffer freed, file closed
_S

// Wrapper functions take void* and cast appropriately
void fclose_wrapper(void* ptr) {
    FILE** fp = (FILE**)ptr;
    if (*fp) fclose(*fp);
}

void free_wrapper(void* ptr) {
    void** p = (void**)ptr;
    if (*p) free(*p);
}
```

## Usage Guide

Generally, everything that's only used in the current scope or subscopes should
have deferred cleanup, while things that you intend to give to a higher scope
should have errdeferred cleanup, meaning you don't intend to clean them up yet,
but if something goes wrong and you can't build your multi-part widget and
return it, THEN you need to clean up the pieces.

local lifetime: use defer  
nonlocal lifetime: use errdefer  

Although the performance impact is small, you may consider using this library
to quickly prototype working cleanup logic. Then, you might later revisit your
functions and write the traditional and leaner goto cleanup blocks, using the
defer and errdefer labels to give you a clear mental model in writing accurate and safe manual
cleanup code that mirrors the defer version.

If you're not overriding any keywords and have to pick the right versions to use:
You can safely use the all caps versions any time, but only have to when inside a
`S_ _S` scope. Even inside `S_ { } _S` you still need the all caps keywords. You
can't defer/errdefer safely inside an unsupported scope nested in a supported
scope, but the keywords in there still need to be defer aware forms. You can also
RETURNERR from inside a `S_ { } _S` type of scope.

### Basic Defer

Defer executes cleanup in LIFO (Last In, First Out) order:

```c
void process_file() S_
    int a = 1;
    defer(cleanup_a, a);
    
    int b = 2;
    defer(cleanup_b, b);
    
    // On scope exit: cleanup_b(b) then cleanup_a(a)
_S
```

### Error Handling with errdefer

`errdefer` only runs when using `returnerr`:

```c
int open_resources() S_
    Resource* r1 = acquire_resource();
    errdefer(release_resource, r1);  // Only runs on error
    
    Resource* r2 = acquire_resource();
    errdefer(release_resource, r2);  // Only runs on error
    
    if (something_failed) {
        returnerr -1;  // Both errdefers execute
    }
    
    return 0;  // Normal return - errdefers DON'T execute
_S
```

### Cleanup at Declaration

For simple cases, use `cleanupdecl`, it has less overhead in gnu c, directly
using __attribute__((cleanup)). `cleanupdecl` isn't unbraced if/while/for safe
in C99. So use `S_ _S` braces there with `cleanupdecl`. Avoid implicit scopes on
those statements in general.

```c
S_
    // Allocate and register cleanup in one line
    FILE* cleanupdecl(f, fopen("data.txt", "r"), fclose_wrapper);
    
    // Use f normally...
_S  // Automatically closed
```

## Writing Cleanup Functions

Cleanup functions must have this signature:

```c
void cleanup_func(void* ptr);
```

The pointer points to your variable (passed by reference):

```c
void cleanup_int(void* ptr) {
    int* val = (int*)ptr;
    printf("Cleaning up: %d\n", *val);
}

void cleanup_file(void* ptr) {
    FILE** fp = (FILE**)ptr;
    if (*fp) {
        fclose(*fp);
        *fp = NULL;
    }
}

void cleanup_allocated(void* ptr) {
    void** p = (void**)ptr;
    if (*p) {
        free(*p);
        *p = NULL;
    }
}
```

**Important**: Cleanup functions receive a pointer to your variable, not the variable itself. Variables are captured by reference, so cleanup sees the current value at scope exit.

## Best Practices

### Do's

* Use defer for cleanup that should always happen  
* Use errdefer for cleanup that should only happen on error  
* Use defer features only directly under special scopes with `S_` `_S`  
* Always use braces with if/for/while statements, `{}` or `S_ _S`  
* Use wrapper cleanup functions that take `void*` and cast internally  
* Check for NULL before cleanup if needed  

### Don'ts

* Don't use defer features outside `S_` `_S` scopes  
* Don't use defer/errdefer in any normal scope nested under a `S_` `_S` scope.  
  * `if (cond) defer(func, arg);` <- WRONG  
  * Make scopes explicit by always bracing if/for/while statements with `{}` or `S_ _S`  
* Don't try to conditionally defer (use errdefer).  
* Don't manually free something that has deferred cleanup unless cleanup is null-safe.  
* Don't accidentally use defer features in an implicit and unsupported scope (Unbraced if/for/while, etc.)  
* Don't use goto, interleaved switch statements, or longjmp, to jump in or out of defer scopes  
* Don't use side effects in return expressions.  


## Keyword redefinition situation (very spooky)
Keywords may be redefined as macros depending on compiler features and the chosen
configuration of this library.

* GNUC compatible compiler: No keyword redefinitions :)
* No GNUC, no configuration: Global redefinition of control flow keywords.
* No GNUC, macro stack feature enabled: Keywords are conditionally expanded only in defer scopes
* No GNUC, DONT_OVERRIDE_KEYWORDS defined: No keyword redefinitions :) Use all caps versions of
    RETURN, RETURNERR, FOR, WHILE, DO, BREAK, and CONTINUE, when under a `S_ _S` scope.

Redefined keywords are expected to be 100% compatible and macro hygienic, and not cause any issues
in existing code.

### You should actually prefer that keywords get redefined in the C99 version, at least in defer scopes via the macro stack feature

Here's why: 

* Manually having to decide which versions of keywords to use is error prone.
* 3rd party macros will interact correctly with defer logic when keywords are
overridden.
* It maintains behavior parity between GNUC and C99 uses of this library.
* Zlib compiles and passes its own tests with its keywords globally overwritten. (Try it yourself with `make zlib-test`!)

Consider an opaque third‑party macro that wraps user code which includes deferred code and `break`, in which the macro internally uses a for loop. That macro has no idea this library exists, so it will naturally use the built‑in keywords rather than any custom RETURN/BREAK/CONTINUE variants.
If this library didn’t redefine the keywords, any defer scopes inside such a macro would be unaware of the internal loop and clean up too much deferred functions immediately, all the way to the next known loop/switch statement. A break inside the macro would bypass my cleanup logic, and scope‑based cleanup would get out of sync.
Redefining return, break, continue, for, while, and switch to wrapper macros, ensures that all control flow—user code and opaque macro bodies—flows through the same instrumentation. That means:

* The defer system always knows when a loop is active.
* break and continue from inside third‑party macros correctly trigger the right level of cleanup, instead of accidentally cleaning up too much.

In other words, the keyword redefinitions make this defer engine transparent and macro‑safe, even for code that wasn’t written with this library in mind.
Only when you explicitly opt out (e.g. with DONT_REDEFINE_KEYWORDS) do you have to manually use uppercase RETURN, BREAK, etc., and accept that macros using the plain keywords might not interact correctly with the defer scopes. Now consider the surprise when such code compiles correctly under
GNUC but only with a C99 compiler, deferred functions subtly run at inappropriate times because keywords were not redefined.

### Keyword redefinition example:

```c
#define return if (execute_all_defers(&_ctx), 0) {} else return
```

This odd macro pattern is perfectly macro hygienic. It chains correctly with
preceding unbraced if/for/while statements. It doesn't cause dangling else
issues, thanks to the inclusion of its own else. And in the end your return 
expression ends up right after the return keyword, just like you expected. As
far as I can tell and test, nothing can go wrong redefining it this way. There's
also a global null _ctx that allows this to work outside of return scopes. The
other keyword redefinitions are similar.

## Installation

### Basic Setup (C99+ portable)

1. Copy `defer.h` to your project
2. Include it:
```c
#define USE_C99_DEFER  // To force c99 version (GNU version is better)
#include "defer.h"
```

### Advanced Setup (C99 Macro Stack for keyword safety)
Scope aware conditional expansion of macros. Did I invent this? I don't know, but it requires push_macro support, so it's not pure C99. Support is automatically detected at compile time.  

For the C99 version with keyword redefinition limited to the special `S_ _S` defer
scopes, enable the macro stack feature. You don't have to know exactly how it
works, but it's a preprocessor hack to allow a macro to detect whether it's in
our special scopes or not. It has a significant drawback, there is a predefined
cap on the number of independant `S_ _S` scopes you can use. (Nested scopes don't
count towards the limit. Scopes only count at a lexical level, a function with
one defer aware scope counts only once towards the limit even if called many
times.)

1. Generate a macro stack header with support for 1000 scopes:
```bash
# fallback means keywords start getting globally redefined again after the 1000th
# top level `S_ _S` scope
./make_macro_stack.sh 1000 fallback > macro_stack.h
# fail means exceeding 1000 top level `S_ _S` will cause a compilation error.
./make_macro_stack.sh 1000 fail > macro_stack.h
```

1. Include in your code before defer.h:
```c
#include "macro_stack.h"
#define USE_C99_DEFER
#include "defer.h"
```

The macro stack limits keyword redefinition to only active defer scopes,
reducing runtime overhead and global keyword pollution.

## API Reference

### Scope Delimiters

- `S_` - Begin a defer-aware scope
- `_S` - End a defer-aware scope (executes all defers)

### Cleanup Registration

- `defer(cleanup_func, variable)` - Always runs cleanup on scope exit
- `errdefer(cleanup_func, variable)` - Only runs if `returnerr` is used
- `cleanupdecl(name, value, cleanup_func)` - Declare and register in one step

### Control Flow

When inside `S_` `_S` scopes:

- `return` - Executes all defers in all scopes up to function level
- `returnerr` - Like return, but marks scope as "error" (triggers errdefers)
- `break` - Executes defers up to the loop/switch being broken
- `continue` - Executes defers up to the loop being continued
- `for`/`do`/`while` - Records a checkpoint for break and continue to cleanup to
- `switch` - Records a checkpoint for only break to cleanup to

**Note**: If `DONT_REDEFINE_KEYWORDS` is defined, use uppercase versions: `RETURN`, `RETURNERR`, `BREAK`, `CONTINUE`, `FOR`, `DO`, `WHILE`, `SWITCH`.

## Implementation Details

### GNU C Version (`__GNUC__` defined)

- Uses `__attribute__((cleanup))` for automatic cleanup
- Minimal runtime overhead
- Redefines keywords to track scope context (can be limited with macro stack)

### C99 Portable Version (`USE_C99_DEFER` defined)

- Uses stack allocated linked list to track defers
- No heap usage
- Low runtime overhead
- Fully portable to any C99+ compiler

Both implementations:
- Are fully reentrant and thread-safe (no global state)
- Handle arbitrarily nested scopes
- Work with recursive functions
- Support all standard control flow constructs

### Known differences in behavior:
* When a return expression (either with return or returnerr) has side effects,
those side effects occur after deferred cleanup in C99, but before deferred
cleanup in GNU C.
* cleanupdecl should never be used in the implicit scope of an unbraced if/for/while
statement anyway, but doing so in GNUC means cleanup happens earlier than expected, vs in
C99 the macro is macro unhygienic, and you end up trying to cleanup a dead value from
that implicit scope at the end of the enclosing scope.

## Testing

Run the test suite:

```bash
# Test C99 version
make run-test-c99

# Test GNU C version
make run-test-gnu

# With macro stack
make run-test-c99-macro
```
* Tested with GCC, clang, and tcc, using fsanitize=undefined,address  
* MSVC and other C99+ compilers are expected to work fine.  

The test suite includes 42 tests covering:
- Basic defer and scope management
- Error handling with errdefer
- Complex control flow (loops, switches, nested structures)
- Edge cases and pathological nesting
- Recursion and reentrancy

### Bonus test:

```bash
make zlib-test
```

**This will clone zlib, inject defer.h in C99 mode into all .h files inside,
globally redefining keywords throughout zlib, and then it builds and runs
zlib's own test program, which will pass. The keyword redefinitions are safe.**
