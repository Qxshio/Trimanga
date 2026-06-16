CXX ?= clang++
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/trimanga

CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/stb
LDFLAGS ?=

SOURCES := \
	src/main.cpp \
	src/classifier.cpp \
	src/file_utils.cpp \
	src/image/image_loader.cpp \
	src/image/page_features.cpp \
	src/process.cpp \
	src/scanner.cpp \
	src/detectors/scanlation_detector.cpp

OBJECTS := $(patsubst src/%,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

$(BUILD_DIR)/%.cpp.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.mm.o: src/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -x objective-c++ -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
