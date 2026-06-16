CXX ?= clang++
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/trimanga

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(strip $(SDL_LIBS)),)
PREVIEW_SOURCE := src/preview/previewer_stub.cpp
PREVIEW_CXXFLAGS :=
PREVIEW_LDFLAGS :=
else
PREVIEW_SOURCE := src/preview/sdl_previewer.cpp
PREVIEW_CXXFLAGS := $(SDL_CFLAGS) -DTRIMANGA_WITH_SDL=1
PREVIEW_LDFLAGS := $(SDL_LIBS)
endif

CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/stb $(PREVIEW_CXXFLAGS)
LDFLAGS ?= $(PREVIEW_LDFLAGS)

SOURCES := \
	src/main.cpp \
	src/classifier.cpp \
	src/file_utils.cpp \
	src/image/image_loader.cpp \
	src/image/page_features.cpp \
	src/process.cpp \
	src/scanner.cpp \
	src/detectors/scanlation_detector.cpp \
	$(PREVIEW_SOURCE)

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
