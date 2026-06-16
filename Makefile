CXX ?= clang++
CC ?= cc
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/trimanga
UNAME_S := $(shell uname -s)

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_DYLIB := $(firstword $(wildcard $(SDL_PREFIX)/lib/libSDL2-2.0.0.dylib) $(wildcard $(SDL_PREFIX)/lib/libSDL2.dylib))
ifeq ($(strip $(SDL_LIBS)),)
PREVIEW_SOURCE := src/preview/previewer_stub.cpp
PREVIEW_CXXFLAGS :=
PREVIEW_LDFLAGS :=
else
PREVIEW_SOURCE := \
	src/preview/sdl_previewer.cpp \
	src/preview/sdl/drawing.cpp \
	src/preview/sdl/layout.cpp \
	src/preview/sdl/page_card.cpp \
	src/preview/sdl/texture_cache.cpp \
	src/preview/sdl/widgets.cpp
PREVIEW_CXXFLAGS := $(SDL_CFLAGS) -DTRIMANGA_WITH_SDL=1
PREVIEW_LDFLAGS := $(SDL_LIBS)
endif

CFLAGS ?= -Wall -Wextra -Ithird_party/miniz
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/stb -Ithird_party/miniz $(PREVIEW_CXXFLAGS)
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

THIRD_PARTY_C_SOURCES := \
	third_party/miniz/miniz.c \
	third_party/miniz/miniz_tdef.c \
	third_party/miniz/miniz_tinfl.c \
	third_party/miniz/miniz_zip.c

OBJECTS := $(patsubst src/%,$(BUILD_DIR)/%.o,$(SOURCES)) \
	$(patsubst third_party/%,$(BUILD_DIR)/third_party/%.o,$(THIRD_PARTY_C_SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
ifeq ($(UNAME_S),Darwin)
ifneq ($(strip $(SDL_DYLIB)),)
	@mkdir -p $(BUILD_DIR)/lib
	rm -f "$(BUILD_DIR)/lib/$(notdir $(SDL_DYLIB))"
	cp "$(SDL_DYLIB)" "$(BUILD_DIR)/lib/$(notdir $(SDL_DYLIB))"
	@install_name=$$(otool -D "$(SDL_DYLIB)" | tail -n 1); \
	install_name_tool -change "$$install_name" "@executable_path/lib/$(notdir $(SDL_DYLIB))" "$@"
endif
endif

$(BUILD_DIR)/%.cpp.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/%.mm.o: src/%.mm
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -x objective-c++ -c $< -o $@

$(BUILD_DIR)/third_party/%.c.o: third_party/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
