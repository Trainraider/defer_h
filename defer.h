#ifndef DEFER_H
#define DEFER_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define _CAT_IMPL(a, b) a##b
#define _CAT(a, b) _CAT_IMPL(a, b)

#ifdef __has_attribute
  #define _attribute(x) __attribute__(x)
#else
  #define _attribute(x)
#endif

#ifdef __COUNTER__
  #define _UNIQUER __COUNTER__
#else
  #define _UNIQUER __LINE__
#endif

//#define USE_C99_DEFER

#if defined (__GNUC__) && !defined(USE_C99_DEFER)

typedef struct _dfr_DeferNode {
    void (*func)(void*);
    void* arg;
} _dfr_DeferNode;

typedef struct _dfr_ErrDeferNode {
    void (*func)(void*);
    void* arg;
    bool* err_occurred;
} _dfr_ErrDeferNode;

static void _dfr_execute_defer (_dfr_DeferNode* node) {
    node->func(node->arg);
} 

static void _dfr_execute_errdefer (_dfr_ErrDeferNode* node) {
    if (*node->err_occurred) node->func(node->arg);
} 

#define S_ { bool _dfr_err __attribute__((unused)) = false;
#ifdef __clang__
#define _S _Pragma("GCC diagnostic push") \
    _Pragma("GCC diagnostic ignored \"-Wreturn-type\"") \
    } _Pragma("GCC diagnostic pop")
#else
#define _S }
#endif

#define defer(cleanup_func, var) \
    _dfr_DeferNode _CAT(_defer, __COUNTER__) __attribute__((cleanup(_dfr_execute_defer))) = \
    (_dfr_DeferNode){.func = cleanup_func, .arg = &var};

// If you can defer at declaration time, this is lighter than defer
#define cleanupdecl(lvalue, rvalue, cleanup_fn) lvalue __attribute__((cleanup(cleanup_fn))) = rvalue

#define errdefer(cleanup_func, var) \
    _dfr_ErrDeferNode _CAT(_defer, __COUNTER__) __attribute__((cleanup(_dfr_execute_errdefer))) = \
    (_dfr_ErrDeferNode){.func = cleanup_func, .arg = &var, .err_occurred = &_dfr_err};

#define returnerr if (( _dfr_err = true), 0) {} else return

#ifdef DONT_REDEFINE_KEYWORDS
#define RETURN return
#define RETURNERR returnerr
#define BREAK break
#define CONTINUE continue
#define FOR for
#define DO do
#define WHILE while
#define SWITCH switch

#endif // DONT_REDEFINE_KEYWORDS

#else

typedef struct _dfr_DeferNode {
    struct _dfr_DeferNode* next;
    bool is_err;
    void (*func)(void*);
    void* arg;
} _dfr_DeferNode;

typedef struct _dfr_ScopeCtx {
    bool error_occurred;
    _dfr_DeferNode* head;
    _dfr_DeferNode* old_head;
    struct _dfr_ScopeCtx* parent;
} _dfr_ScopeCtx;

// Global dummy contexts allow keyword macros
// to function outside of S_ _S scopes
// They are effectively const even if not declared as such,
// being NULL at all times.
static _dfr_ScopeCtx* const _dfr_ctx = NULL;
static _dfr_ScopeCtx* _dfr_break_ctx = NULL;
static _dfr_ScopeCtx* _dfr_continue_ctx = NULL;

static inline void _dfr_execute_defers(_dfr_ScopeCtx* ctx) {
    if (!ctx) return;
    bool error_occurred = ctx->error_occurred;
    _dfr_DeferNode* node = ctx->head;
    if (error_occurred) {
        while(node) {
            node->func(node->arg);
            node = node->next;
        }
    } else {
        while(node) {
            if (!node->is_err) {
                node->func(node->arg);
            }
            node = node->next;
        }
    }
}

static inline void _dfr_execute_all_defers(_dfr_ScopeCtx* ctx) {
    if (!ctx) return;
    bool error_occurred = ctx->error_occurred;
    for (_dfr_ScopeCtx* current = ctx; current; current = current->parent) {
        _dfr_DeferNode* node = current->head;
        if (error_occurred) {
            while(node) {
                node->func(node->arg);
                node = node->next;
            }
        } else {
            while(node) {
                if (!node->is_err) {
                    node->func(node->arg);
                }
                node = node->next;
            }
        }
    }
}

static inline void _dfr_execute_some_defers(_dfr_ScopeCtx* start, _dfr_ScopeCtx* end) {
    if (!start) return;
    if (!end) {
        _dfr_execute_all_defers(start);
        return;
    };
    for (_dfr_ScopeCtx* current = start; current && current != end; current = current->parent) {
        _dfr_DeferNode* node = current->head;
        while(node) {
            if (!node->is_err) {
                node->func(node->arg);
            }
            node = node->next;
        }
    }
}

static inline _dfr_ScopeCtx* _dfr_scope_helper(_dfr_ScopeCtx* _dfr_ctx) {
    _dfr_execute_defers(_dfr_ctx);
    return NULL;
}

#define S_ { \
    _dfr_ScopeCtx* _dfr_parent_break_ctx = _dfr_break_ctx; \
    _dfr_ScopeCtx* _dfr_break_ctx = _dfr_parent_break_ctx; \
    _dfr_ScopeCtx* _dfr_parent_continue_ctx = _dfr_continue_ctx; \
    _dfr_ScopeCtx* _dfr_continue_ctx = _dfr_parent_continue_ctx; \
    _dfr_ScopeCtx _dfr_ctx_ = (_dfr_ScopeCtx){ false, NULL, NULL, _dfr_ctx}, *_dfr_ctx = &_dfr_ctx_;
#ifdef __clang__
#define _S ;_Pragma("clang diagnostic push") \
    _Pragma("clang diagnostic ignored \"-Wreturn-type\"") \
    _dfr_scope_helper(_dfr_ctx);} \
    _Pragma("clang diagnostic pop")
#else
#define _S ; _dfr_scope_helper(_dfr_ctx); }
#endif

#define _CAT_IMPL(a, b) a##b
#define _CAT(a, b) _CAT_IMPL(a, b)

static inline _dfr_DeferNode* link_defer_node(_dfr_DeferNode** old_head, _dfr_DeferNode** new_node) {
    _dfr_DeferNode* ret = *old_head;
    *old_head = *new_node;
    return ret;
}

// This macro has carefully been made unbraced if safe, as far as macro hygiene
// goes anyways. But the lifetime of the node ends early if used in an if
// statement without a proper S_ _S scope. It'll probably work, but it's undefined
// behavior. You don't need to conditionally defer though. Just use errdefer
// instead.
#define _dfr_defer_impl(cleanup_func, var, err, unique) \
    _dfr_DeferNode _CAT(node, unique) = ( \
        (_dfr_ctx_.head = &_CAT(node, unique)), \
        (_dfr_DeferNode){ \
            .next = link_defer_node(&_dfr_ctx_.old_head, &_dfr_ctx_.head), \
            .is_err = err, \
            .func = cleanup_func, \
            .arg = &(var) \
        } \
    )

#define _dfr_defer(cleanup_func, var, err) \
    _dfr_defer_impl(cleanup_func, var, err, _UNIQUER)

#define defer(cleanup_func, var) _dfr_defer(cleanup_func, var, false)
#define errdefer(cleanup_func, var) _dfr_defer(cleanup_func, var, true)

// Included for compatibility with gnuc path, but this version is
// macro unhygienic! Don't use it with unbraced if/for/while! (Though that's 
// user error anyway due to implicit scope creation of those statements
// interfering with defer scopes.)
#define cleanupdecl(lvalue, rvalue, cleanup_fn) lvalue = rvalue; \
    _dfr_defer(cleanup_fn, lvalue, false)

static inline int _dfr_loop_helper(_dfr_ScopeCtx** _dfr_break_ctx, _dfr_ScopeCtx** _dfr_continue_ctx, _dfr_ScopeCtx* _dfr_ctx) {
    *_dfr_break_ctx = _dfr_ctx;
    *_dfr_continue_ctx = _dfr_ctx;
    return 1;
}

static inline int _dfr_switch_helper(_dfr_ScopeCtx** _dfr_break_ctx, _dfr_ScopeCtx* _dfr_ctx) {
    *_dfr_break_ctx = _dfr_ctx;
    return 1;
}

#ifndef DONT_REDEFINE_KEYWORDS

#define PUSH_MACRO_SUPPORTED 1
#pragma push_macro("PUSH_MACRO_SUPPORTED")
#define PUSH_MACRO_SUPPORTED 0
#pragma pop_macro("PUSH_MACRO_SUPPORTED")

#if PUSH_MACRO_SUPPORTED && defined(SCOPE_MACRO_STACK_AVAILABLE)
// This is the let's try to mostly not globally redefine keywords version
/* Example macro stack to provide via generated header:
#define IN_SCOPE 1
#pragma push_macro("IN_SCOPE")
#define IN_SCOPE 0
#pragma push_macro("IN_SCOPE")
#define IN_SCOPE 1
#pragma push_macro("IN_SCOPE")
#define IN_SCOPE 0
// Must end with #define IN_SCOPE 0
// The start value for IN_SCOPE controls behavior at stack exhaustion.
// Start value 1: keywords start getting always overridden then on.
// Start values other than 0/1: Ungraceful wall of compilation errors only on
// stack exhaustion.
// Start value ERROR_DEFER_SCOPE_STACK_DEPLETED: Wall of explanatory compilation errors
// on stack exhaustion. It doesn't matter of the compiler supports this error pragma,
// this error text will be all over the error output anyway.
// When the stack is depleted with start value 1,
// This should still work but adds marginal overhead.
// Each top level defer scope consumes two stack values.
// Nested scopes consume nothing.

// And remember to define SCOPE_MACRO_STACK_AVAILABLE in your header.
*/
#define S_ERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")

#define S_0 { _Pragma("pop_macro(\"IN_SCOPE\")"); \
    _dfr_ScopeCtx* _dfr_parent_break_ctx = _dfr_break_ctx; \
    _dfr_ScopeCtx* _dfr_break_ctx = _dfr_parent_break_ctx; \
    _dfr_ScopeCtx* _dfr_parent_continue_ctx = _dfr_continue_ctx; \
    _dfr_ScopeCtx* _dfr_continue_ctx = _dfr_parent_continue_ctx; \
    _dfr_ScopeCtx _dfr_ctx_ = (_dfr_ScopeCtx){ false, NULL, NULL, _dfr_ctx}, *_dfr_ctx = &_dfr_ctx_;

#define S_1 { _Pragma("push_macro(\"IN_SCOPE\")"); \
    _dfr_ScopeCtx* _dfr_parent_break_ctx = _dfr_break_ctx; \
    _dfr_ScopeCtx* _dfr_break_ctx = _dfr_parent_break_ctx; \
    _dfr_ScopeCtx* _dfr_parent_continue_ctx = _dfr_continue_ctx; \
    _dfr_ScopeCtx* _dfr_continue_ctx = _dfr_parent_continue_ctx; \
    _dfr_ScopeCtx _dfr_ctx_ = (_dfr_ScopeCtx){ false, NULL, NULL, _dfr_ctx}, *_dfr_ctx = &_dfr_ctx_;

#undef S_
#define S_ _CAT(S_, IN_SCOPE)


#undef _S
#define _S ; _dfr_scope_helper(_dfr_ctx); _Pragma("pop_macro(\"IN_SCOPE\")"); }

#define returnERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define return0 return
#define return1 if ((_dfr_execute_all_defers(_dfr_ctx)), 0) {} else return
#define return _CAT(return, IN_SCOPE)

#define returnerr if ((_dfr_ctx_.error_occurred = true), 0) {} else return

#define breakERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define break0 break
#define break1 if (_dfr_execute_some_defers(_dfr_ctx, _dfr_break_ctx), 0) {} else break
#define break _CAT(break, IN_SCOPE)

#define continueERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define continue0 continue
#define continue1 if (_dfr_execute_some_defers(_dfr_ctx, _dfr_continue_ctx), 0) {} else continue
#define continue _CAT(continue, IN_SCOPE)

#define doERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define do0 do
#define do1 if (_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), 0) {} else do
#define do _CAT(do, IN_SCOPE)

#define forERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define for0 for
#define for1 if (_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), 0) {} else for
#define for _CAT(for, IN_SCOPE)

#define whileERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define while0(...) while(__VA_ARGS__)
#define while1(...) while(_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), (__VA_ARGS__))
#define while(...) _CAT(while, IN_SCOPE)(__VA_ARGS__)

#define switchERROR_DEFER_SCOPE_STACK_DEPLETED \
 _Pragma("GCC error \"defer.h macro stack exhausted. Consider increasing the \
macro_stack.h size via `./make_macro_stack.sh 9999` or don't #include it anymore\"")
#define switch0 switch
#define switch1 if (_dfr_switch_helper(&_dfr_break_ctx, _dfr_ctx), 0) {} else switch
#define switch _CAT(switch, IN_SCOPE)

#else // push_macro not supported or no SCOPE_MACRO_STACK_AVAILABLE

// keyword wrappers are unbraced if/for/while safe and perfectly compatible
// anywhere keywords are used (As far as I can conceive and have tested), 
// thanks to the static global dummy variables.

#define return if (_dfr_execute_all_defers(_dfr_ctx), 0) {} else return
#define returnerr if ((_dfr_ctx_.error_occurred = true), 0) {} else return
#define break if (_dfr_execute_some_defers(_dfr_ctx, _dfr_break_ctx), 0) {} else break
#define continue if (_dfr_execute_some_defers(_dfr_ctx, _dfr_continue_ctx), 0) {} else continue
#define for if (_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), 0) {} else for
#define do if (_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), 0) {} else do
#define while(...) while(_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, _dfr_ctx), (__VA_ARGS__))
#define switch if (_dfr_switch_helper(&_dfr_break_ctx, _dfr_ctx), 0) {} else switch
#endif // PUSH_MACRO_SUPPORTED
#else
#define RETURN if (_dfr_execute_all_defers(_dfr_ctx), 0) {} else return
#define RETURNERR if ((_dfr_ctx_.error_occurred = true), _dfr_execute_all_defers(_dfr_ctx), 0) {} else return
#define BREAK if (_dfr_execute_some_defers(_dfr_ctx, _dfr_break_ctx), 0) {} else break
#define CONTINUE if (_dfr_execute_some_defers(_dfr_ctx, _dfr_continue_ctx), 0) {} else continue
#define FOR if (_dfr_loop_helper(_dfr_break_ctx, _dfr_continue_ctx, _dfr_ctx), 0) {} else for
#define DO if (_dfr_loop_helper(_dfr_break_ctx, _dfr_continue_ctx, _dfr_ctx), 0) {} else do
#define WHILE(...) while(_dfr_loop_helper(&_dfr_break_ctx, &_dfr_continue_ctx, &_dfr_ctx), (__VA_ARGS__))
#define SWITCH if (_dfr_switch_helper(&_dfr_break_ctx, &_dfr_ctx), 0) {} else switch
#endif // DONT_REDEFINE_KEYWORDS
#endif // __GNUC__
#endif // DEFER_H
