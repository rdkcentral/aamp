# Makefile for qtdemuxAnalyzer
# Requires GStreamer development packages

CXX = g++
TARGET = qtdemuxAnalyzer
SOURCE = qtdemuxAnalyzer.cpp

# Platform detection
UNAME_S := $(shell uname -s)

# Set PKG_CONFIG_PATH for macOS
PKG_CONFIG_PATH_MAC := /Library/Frameworks/GStreamer.framework/Versions/1.0/lib/pkgconfig:/opt/homebrew/lib/pkgconfig

# GStreamer pkg-config flags with platform-specific handling
ifeq ($(UNAME_S),Darwin)
    # macOS - use explicit PKG_CONFIG_PATH
    GSTREAMER_CFLAGS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH_MAC)" pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 2>/dev/null)
    GSTREAMER_LIBS := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH_MAC)" pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 2>/dev/null)
else
    # Linux/other platforms
    GSTREAMER_CFLAGS := $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)
    GSTREAMER_LIBS := $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)
endif

# Compiler flags
CXXFLAGS = -Wall -Wextra -std=c++11 $(GSTREAMER_CFLAGS) $(EXTRA_INCLUDES)
LDFLAGS = $(GSTREAMER_LIBS) $(EXTRA_LIBS)

# Build target
$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGET)