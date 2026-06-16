CXX ?= clang++
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/trimanga

CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -DTRIMANGA_APPLE=1
LDFLAGS ?= -framework Foundation -framework Vision -framework CoreImage -framework CoreGraphics -framework ImageIO

SOURCES := \
	src/main.cpp \
	src/classifier.cpp \
	src/file_utils.cpp \
	src/ocr.cpp \
	src/process.cpp \
	src/scanner.cpp \
	src/tesseract_backend.cpp \
	src/apple_vision_backend.mm \
	src/image_features_macos.mm \
	src/trimanga_ocr_backend.mm

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
