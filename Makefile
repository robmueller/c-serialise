CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -I./include
LDFLAGS = -lrt

SRC_DIR = src
BUILD_DIR = build
EXAMPLES_DIR = examples

# Source files
KVSTORE_SRCS = $(SRC_DIR)/kvstore.c $(SRC_DIR)/kvstore_mem.c
KVSTORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(KVSTORE_SRCS))

# Examples
EXAMPLES = $(BUILD_DIR)/kvstore_example \
           $(BUILD_DIR)/kvstore_complex_test \
           $(BUILD_DIR)/index_record_example \
           $(BUILD_DIR)/nested_struct_example

.PHONY: all clean examples

all: $(BUILD_DIR) $(EXAMPLES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build kvstore example
$(BUILD_DIR)/kvstore_example: $(EXAMPLES_DIR)/kvstore_example.c $(KVSTORE_OBJS) include/*.h
	$(CC) $(CFLAGS) $< $(KVSTORE_OBJS) -o $@ $(LDFLAGS)

# Build complex kvstore test
$(BUILD_DIR)/kvstore_complex_test: $(EXAMPLES_DIR)/kvstore_complex_test.c $(KVSTORE_OBJS) include/*.h
	$(CC) $(CFLAGS) $< $(KVSTORE_OBJS) -o $@ $(LDFLAGS)

# Build original index_record example
$(BUILD_DIR)/index_record_example: $(EXAMPLES_DIR)/index_record_example.c include/serialise.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

# Build nested struct example
$(BUILD_DIR)/nested_struct_example: $(EXAMPLES_DIR)/nested_struct_example.c include/serialise.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

examples: $(EXAMPLES)

clean:
	rm -rf $(BUILD_DIR)

# Run examples
run-kvstore: $(BUILD_DIR)/kvstore_example
	./$(BUILD_DIR)/kvstore_example

run-complex: $(BUILD_DIR)/kvstore_complex_test
	./$(BUILD_DIR)/kvstore_complex_test

run-index: $(BUILD_DIR)/index_record_example
	./$(BUILD_DIR)/index_record_example

run-nested: $(BUILD_DIR)/nested_struct_example
	./$(BUILD_DIR)/nested_struct_example

run-all: $(EXAMPLES)
	@echo "=== Running index_record_example ==="
	@./$(BUILD_DIR)/index_record_example
	@echo ""
	@echo "=== Running kvstore_example ==="
	@./$(BUILD_DIR)/kvstore_example
	@echo ""
	@echo "=== Running kvstore_complex_test ==="
	@./$(BUILD_DIR)/kvstore_complex_test
	@echo ""
	@echo "=== Running nested_struct_example ==="
	@./$(BUILD_DIR)/nested_struct_example
