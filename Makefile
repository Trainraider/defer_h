.PHONY: all demo run clean test-all run-tests help
.PHONY: test-gnu test-c99 test-c99-macro
.PHONY: run-test-gnu run-test-c99 run-test-c99-macro
.PHONY: zlib zlib-test run-test-zlib-keyword-injection

CC = clang
CFLAGS = -std=gnu11
CFLAGS_C99 = -std=c99
CFLAGS_TEST = -fsanitize=undefined,address -g -O0

# Output directories
DIST = dist
DEMO_DIR = $(DIST)/demo
TEST_DIR = $(DIST)/tests

# Default target: build demo
all: $(DEMO_DIR)/demo

# Compile and run demo
run: $(DEMO_DIR)/demo
	$(DEMO_DIR)/demo

# Build demo
demo: $(DEMO_DIR)/demo

$(DEMO_DIR)/demo: demo.c defer.h | $(DEMO_DIR)
	$(CC) $(CFLAGS) -o $(DEMO_DIR)/demo demo.c

$(DEMO_DIR):
	mkdir -p $(DEMO_DIR)

$(TEST_DIR):
	mkdir -p $(TEST_DIR)

# Generate macro_stack.h if needed
macro_stack.h:
	./make_macro_stack.sh 1000 fail > macro_stack.h

# Test targets (suppress warnings during compilation)
$(TEST_DIR)/test_defer_gnu: test_defer.c defer.h | $(TEST_DIR)
	@$(CC) $(CFLAGS) $(CFLAGS_TEST) -o $(TEST_DIR)/test_defer_gnu test_defer.c 2>/dev/null || $(CC) $(CFLAGS) $(CFLAGS_TEST) -o $(TEST_DIR)/test_defer_gnu test_defer.c

$(TEST_DIR)/test_defer_c99: test_defer.c defer.h | $(TEST_DIR)
	@$(CC) $(CFLAGS_C99) $(CFLAGS_TEST) -DUSE_C99_DEFER -o $(TEST_DIR)/test_defer_c99 test_defer.c 2>/dev/null || $(CC) $(CFLAGS_C99) $(CFLAGS_TEST) -DUSE_C99_DEFER -o $(TEST_DIR)/test_defer_c99 test_defer.c

$(TEST_DIR)/test_defer_c99_macro: test_defer.c defer.h macro_stack.h | $(TEST_DIR)
	@$(CC) $(CFLAGS_C99) $(CFLAGS_TEST) -DUSE_C99_DEFER -DUSE_MACRO_STACK -o $(TEST_DIR)/test_defer_c99_macro test_defer.c 2>/dev/null || $(CC) $(CFLAGS_C99) $(CFLAGS_TEST) -DUSE_C99_DEFER -DUSE_MACRO_STACK -o $(TEST_DIR)/test_defer_c99_macro test_defer.c

# Individual test build targets
test-gnu: $(TEST_DIR)/test_defer_gnu

test-c99: $(TEST_DIR)/test_defer_c99

test-c99-macro: $(TEST_DIR)/test_defer_c99_macro

# Individual test run targets
run-test-gnu: $(TEST_DIR)/test_defer_gnu
	@echo "=== Running GNU test ==="
	-$(TEST_DIR)/test_defer_gnu

run-test-c99: $(TEST_DIR)/test_defer_c99
	@echo "=== Running C99 test ==="
	-$(TEST_DIR)/test_defer_c99

run-test-c99-macro: $(TEST_DIR)/test_defer_c99_macro
	@echo "=== Running C99 with macro stack test ==="
	-$(TEST_DIR)/test_defer_c99_macro

# Build all tests
test-all: $(TEST_DIR)/test_defer_gnu $(TEST_DIR)/test_defer_c99 $(TEST_DIR)/test_defer_c99_macro

# Run all tests
run-tests: run-test-gnu run-test-c99 run-test-c99-macro
	@echo "=== All tests completed ==="

# Clean build artifacts
clean:
	rm -rf $(DIST)
	rm -rf macro_stack.h
	rm -rf zlib

# Clone zlib repository (shallow clone of master branch)
zlib:
	git clone --depth=1 --single-branch --branch master https://github.com/madler/zlib.git

# Inject C99 defer keyword macros into zlib headers and run its test suite
zlib-test: zlib defer.h
	cp defer.h zlib/defer.h
	cd zlib && \
	for f in *.h; do \
	  [ "$$f" != "defer.h" ] && sed -i '1i#undef SCOPE_MACRO_STACK_AVAILABLE\n#define USE_C99_DEFER\n#include "defer.h"\n' "$$f"; \
	done && \
	./configure && \
	make test

run-test-zlib-keyword-injection: zlib-test

# Help target
help:
	@echo "Available targets:"
	@echo "  all (default)      - Build demo"
	@echo "  run               - Build and run demo"
	@echo "  demo              - Build demo executable"
	@echo ""
	@echo "Test building:"
	@echo "  test-gnu          - Build test with GNU extensions"
	@echo "  test-c99          - Build test with C99 mode"
	@echo "  test-c99-macro    - Build test with C99 + macro stack"
	@echo "  test-all          - Build all test variants"
	@echo ""
	@echo "Test running:"
	@echo "  run-test-gnu      - Build and run GNU test"
	@echo "  run-test-c99      - Build and run C99 test"
	@echo "  run-test-c99-macro - Build and run C99 macro test"
	@echo "  run-tests         - Build and run all tests"
	@echo "  zlib-test         - Clone and test zlib with injected keyword macros"
	@echo ""
	@echo "  clean             - Remove all build artifacts"
	@echo "  help              - Show this help message"
