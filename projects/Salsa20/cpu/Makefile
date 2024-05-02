EXECUTABLE_NAME = salsa20
SRCS = ./src/cpu.cpp
BUILD_DIR = ./build

EXECUTABLE = $(BUILD_DIR)/$(EXECUTABLE_NAME)

.PHONY: all run clean
all: $(EXECUTABLE)

$(EXECUTABLE): $(SRCS) | $(BUILD_DIR)
	g++ -o $@ $^

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)