CC ?= cc
CFLAGS ?= -g -Og -Wall -Wextra -std=c11 -I../include
LDFLAGS ?=

# Enable sanitizers with: make SAN=1
ifeq ($(SAN),1)
  CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif

all: index_record_example

index_record_example: index_record_example.c ../include/serialise.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

run: index_record_example
	./index_record_example

test: clean
	$(MAKE) SAN=1 index_record_example
	./index_record_example

clean:
	rm -f index_record_example

.PHONY: all run clean
.PHONY: test
