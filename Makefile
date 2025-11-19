CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -I./include -I.
LDFLAGS = -lrt -lcrypto

SRC_DIR = src
BUILD_DIR = build
EXAMPLES_DIR = examples

# Source files
KVSTORE_SRCS = $(SRC_DIR)/kvstore.c $(SRC_DIR)/kvstore_mem.c
KVSTORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(KVSTORE_SRCS))

# Parser source files
PARSER_DIR = parsers
PARSER_SRCS = $(PARSER_DIR)/email_address.c $(PARSER_DIR)/mime_parser.c
PARSER_OBJS = $(patsubst $(PARSER_DIR)/%.c,$(BUILD_DIR)/%.o,$(PARSER_SRCS))

# Examples
EXAMPLES = $(BUILD_DIR)/kvstore_example \
           $(BUILD_DIR)/kvstore_complex_test \
           $(BUILD_DIR)/index_record_example \
           $(BUILD_DIR)/nested_struct_example \
           $(BUILD_DIR)/email_address_test \
           $(BUILD_DIR)/mime_parser_test \
           $(BUILD_DIR)/mime_kvstore_example

.PHONY: all clean examples

all: $(BUILD_DIR) $(EXAMPLES)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c include/*.h
	$(CC) $(CFLAGS) -c $< -o $@

# Compile parser files
$(BUILD_DIR)/%.o: $(PARSER_DIR)/%.c $(PARSER_DIR)/*.h
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

# Build email address parser test
$(BUILD_DIR)/email_address_test: $(EXAMPLES_DIR)/email_address_test.c $(BUILD_DIR)/email_address.o $(PARSER_DIR)/*.h
	$(CC) $(CFLAGS) $< $(BUILD_DIR)/email_address.o -o $@ $(LDFLAGS)

# Build MIME parser test
$(BUILD_DIR)/mime_parser_test: $(EXAMPLES_DIR)/mime_parser_test.c $(PARSER_OBJS) $(PARSER_DIR)/*.h
	$(CC) $(CFLAGS) $< $(PARSER_OBJS) -o $@ $(LDFLAGS)

# Build MIME KV store example
$(BUILD_DIR)/mime_kvstore_example: $(EXAMPLES_DIR)/mime_kvstore_example.c $(KVSTORE_OBJS) $(PARSER_OBJS) $(PARSER_DIR)/*.h include/*.h
	$(CC) $(CFLAGS) $< $(KVSTORE_OBJS) $(PARSER_OBJS) -o $@ $(LDFLAGS)

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

run-email: $(BUILD_DIR)/email_address_test
	./$(BUILD_DIR)/email_address_test

run-mime: $(BUILD_DIR)/mime_parser_test
	./$(BUILD_DIR)/mime_parser_test

run-mime-kv: $(BUILD_DIR)/mime_kvstore_example
	./$(BUILD_DIR)/mime_kvstore_example

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
	@echo ""
	@echo "=== Running email_address_test ==="
	@./$(BUILD_DIR)/email_address_test
	@echo ""
	@echo "=== Running mime_parser_test ==="
	@./$(BUILD_DIR)/mime_parser_test
