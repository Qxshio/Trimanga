CXX ?= clang++
CC ?= cc
BUILD_DIR ?= build
TARGET := $(BUILD_DIR)/trimanga
PREVIEW_TARGET := $(BUILD_DIR)/trimanga-preview
UNAME_S := $(shell uname -s)

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_DYLIB := $(firstword $(wildcard $(SDL_PREFIX)/lib/libSDL2-2.0.0.dylib) $(wildcard $(SDL_PREFIX)/lib/libSDL2.dylib))
TURBOJPEG_PREFIX := $(shell brew --prefix jpeg-turbo 2>/dev/null)
TURBOJPEG_HEADER := $(firstword $(wildcard /opt/homebrew/include/turbojpeg.h) $(wildcard /usr/local/include/turbojpeg.h) $(wildcard /usr/include/turbojpeg.h) $(wildcard $(TURBOJPEG_PREFIX)/include/turbojpeg.h))
TURBOJPEG_DYLIB := $(firstword $(wildcard /opt/homebrew/lib/libturbojpeg.dylib) $(wildcard /usr/local/lib/libturbojpeg.dylib) $(wildcard $(TURBOJPEG_PREFIX)/lib/libturbojpeg.dylib) $(wildcard /usr/local/lib/libturbojpeg.so) $(wildcard /usr/lib/libturbojpeg.so) $(wildcard /usr/lib/*/libturbojpeg.so))
ifneq ($(strip $(TURBOJPEG_HEADER)$(TURBOJPEG_DYLIB)),)
ifneq ($(strip $(TURBOJPEG_HEADER)),)
ifneq ($(strip $(TURBOJPEG_DYLIB)),)
TURBOJPEG_CFLAGS := -DTRIMANGA_WITH_TURBOJPEG=1 -I$(dir $(TURBOJPEG_HEADER))
TURBOJPEG_LIBS := -L$(dir $(TURBOJPEG_DYLIB)) -lturbojpeg
endif
endif
endif
ifeq ($(strip $(SDL_LIBS)),)
PREVIEW_SOURCE := src/preview/previewer_stub.cpp
PREVIEW_TARGETS :=
else
PREVIEW_SOURCE := src/preview/previewer_spawn.cpp src/preview/preview_ipc.cpp
PREVIEW_HELPER_SOURCES := \
	src/preview/sdl_preview_main.cpp \
	src/preview/sdl_previewer.cpp \
	src/preview/preview_ipc.cpp \
	src/image/image_loader.cpp \
	src/preview/sdl/drawing.cpp \
	src/preview/sdl/layout.cpp \
	src/preview/sdl/page_card.cpp \
	src/preview/sdl/texture_cache.cpp \
	src/preview/sdl/widgets.cpp
PREVIEW_TARGETS := $(PREVIEW_TARGET)
endif

OPTFLAGS ?= -O3 -DNDEBUG
CFLAGS ?= $(OPTFLAGS) -Wall -Wextra -Ithird_party/miniz
CXXFLAGS ?= $(OPTFLAGS) -std=c++20 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/stb -Ithird_party/miniz $(TURBOJPEG_CFLAGS)
PREVIEW_CXXFLAGS ?= $(CXXFLAGS) $(SDL_CFLAGS) -DTRIMANGA_WITH_SDL=1
LDFLAGS ?= $(TURBOJPEG_LIBS)
PREVIEW_LDFLAGS ?= $(SDL_LIBS) $(TURBOJPEG_LIBS)

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

PREVIEW_HELPER_OBJECTS := $(patsubst src/%,$(BUILD_DIR)/%.o,$(PREVIEW_HELPER_SOURCES))

.PHONY: all clean

all: $(TARGET) $(PREVIEW_TARGETS)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@
ifeq ($(UNAME_S),Darwin)
ifneq ($(strip $(TURBOJPEG_DYLIB)),)
	@mkdir -p $(BUILD_DIR)/lib
	rm -f "$(BUILD_DIR)/lib/$(notdir $(TURBOJPEG_DYLIB))"
	cp "$(TURBOJPEG_DYLIB)" "$(BUILD_DIR)/lib/$(notdir $(TURBOJPEG_DYLIB))"
	@install_name=$$(otool -D "$(TURBOJPEG_DYLIB)" | tail -n 1); \
	install_name_tool -change "$$install_name" "@executable_path/lib/$(notdir $(TURBOJPEG_DYLIB))" "$@"
endif
endif

$(PREVIEW_TARGET): $(PREVIEW_HELPER_OBJECTS)
	$(CXX) $(PREVIEW_HELPER_OBJECTS) $(PREVIEW_LDFLAGS) -o $@
ifeq ($(UNAME_S),Darwin)
ifneq ($(strip $(TURBOJPEG_DYLIB)),)
	@mkdir -p $(BUILD_DIR)/lib
	rm -f "$(BUILD_DIR)/lib/$(notdir $(TURBOJPEG_DYLIB))"
	cp "$(TURBOJPEG_DYLIB)" "$(BUILD_DIR)/lib/$(notdir $(TURBOJPEG_DYLIB))"
	@install_name=$$(otool -D "$(TURBOJPEG_DYLIB)" | tail -n 1); \
	install_name_tool -change "$$install_name" "@executable_path/lib/$(notdir $(TURBOJPEG_DYLIB))" "$@"
endif
ifneq ($(strip $(SDL_DYLIB)),)
	@mkdir -p $(BUILD_DIR)/lib
	rm -f "$(BUILD_DIR)/lib/$(notdir $(SDL_DYLIB))"
	cp "$(SDL_DYLIB)" "$(BUILD_DIR)/lib/$(notdir $(SDL_DYLIB))"
	@install_name=$$(otool -D "$(SDL_DYLIB)" | tail -n 1); \
	install_name_tool -change "$$install_name" "@executable_path/lib/$(notdir $(SDL_DYLIB))" "$@"
endif
endif

$(BUILD_DIR)/preview/sdl_preview_main.cpp.o: src/preview/sdl_preview_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PREVIEW_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/preview/sdl_previewer.cpp.o: src/preview/sdl_previewer.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PREVIEW_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/preview/sdl/%.cpp.o: src/preview/sdl/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(PREVIEW_CXXFLAGS) -c $< -o $@

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
