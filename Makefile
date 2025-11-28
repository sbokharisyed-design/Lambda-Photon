
CC = clang
CFLAGS = -Wall -Wextra -O2 -std=c17

LLVM_CFLAGS = $(shell llvm-config --cflags)
LLVM_LDFLAGS = $(shell llvm-config --ldflags)
LLVM_LIBS = $(shell llvm-config --libs core native --system-libs)

SRC_DIR = src
BUILD_DIR = build
TARGET = photon.exe

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean release debug test info

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LLVM_LDFLAGS) $(LLVM_LIBS) -lm

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(LLVM_CFLAGS) -c $< -o $@

release: CFLAGS += -O3 -DNDEBUG
release: clean all

debug: CFLAGS += -g -O0 -DDEBUG
debug: clean all

test: $(TARGET)
	@echo "=== Compiling test.lp ==="
	./$(TARGET) examples/test.lp --emit-llvm
	@echo ""
	@echo "=== Test passed ==="

info:
	@echo "CC:          $(CC)"
	@echo "CFLAGS:      $(CFLAGS)"
	@echo "LLVM_CFLAGS: $(LLVM_CFLAGS)"
	@echo "LLVM_LIBS:   $(LLVM_LIBS)"

clean:
	rm -rf $(BUILD_DIR) $(TARGET)