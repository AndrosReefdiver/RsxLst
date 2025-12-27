# Makefile for RSX Files-11 Filesystem Tool (rsxlst)
# Supports both Linux/Unix and Windows (MinGW/Cygwin)

# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
DEBUGFLAGS = -g -DDEBUG
TARGET = rsxlst

# Detect operating system
ifeq ($(OS),Windows_NT)
    # Windows-specific settings
    RM = del /Q
    RMDIR = rmdir /S /Q
    MKDIR = mkdir
    TARGET := $(TARGET).exe
    PATH_SEP = \\
    NULLDEV = NUL
else
    # Unix/Linux-specific settings
    RM = rm -f
    RMDIR = rm -rf
    MKDIR = mkdir -p
    PATH_SEP = /
    NULLDEV = /dev/null
endif

# Directories
SRC_DIR = .
OBJ_DIR = obj
BIN_DIR = bin

# Source files
SOURCES = RsxLst.cpp \
          Rad50.cpp \
          DiskIO.cpp \
          Files11FileSystem.cpp

# Header files
HEADERS = FileStructures.h \
          Rad50.h \
          DiskIO.h \
          Files11FileSystem.h

# Object files
OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)$(PATH_SEP)%.o,$(SOURCES))

# Default target
.PHONY: all
all: $(BIN_DIR)$(PATH_SEP)$(TARGET)

# Create directories if they don't exist
$(OBJ_DIR):
	@$(MKDIR) $(OBJ_DIR) 2>$(NULLDEV) || echo "Object directory exists"

$(BIN_DIR):
	@$(MKDIR) $(BIN_DIR) 2>$(NULLDEV) || echo "Bin directory exists"

# Link the executable
$(BIN_DIR)$(PATH_SEP)$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJECTS)
	@echo "Build complete: $@"

# Compile source files to object files
$(OBJ_DIR)$(PATH_SEP)%.o: $(SRC_DIR)$(PATH_SEP)%.cpp $(HEADERS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug build
.PHONY: debug
debug: CXXFLAGS += $(DEBUGFLAGS)
debug: clean all

# Clean build artifacts
.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	@if exist $(OBJ_DIR) $(RMDIR) $(OBJ_DIR)
	@if exist $(BIN_DIR) $(RMDIR) $(BIN_DIR)
else
	@$(RMDIR) $(OBJ_DIR) $(BIN_DIR)
endif
	@echo "Clean complete"

# Install (copy to system path) - Unix/Linux only
.PHONY: install
install: all
ifeq ($(OS),Windows_NT)
	@echo "Install target not supported on Windows"
	@echo "Manually copy $(BIN_DIR)$(PATH_SEP)$(TARGET) to your desired location"
else
	install -m 755 $(BIN_DIR)/$(TARGET) /usr/local/bin/
	@echo "Installed to /usr/local/bin/$(TARGET)"
endif

# Uninstall - Unix/Linux only
.PHONY: uninstall
uninstall:
ifeq ($(OS),Windows_NT)
	@echo "Uninstall target not supported on Windows"
else
	$(RM) /usr/local/bin/$(TARGET)
	@echo "Uninstalled from /usr/local/bin/$(TARGET)"
endif

# Run the program with test arguments
.PHONY: run
run: all
	@echo "Running $(TARGET)..."
	@$(BIN_DIR)$(PATH_SEP)$(TARGET)

# Show help
.PHONY: help
help:
	@echo "RSX Files-11 Filesystem Tool - Makefile targets:"
	@echo ""
	@echo "  make                - Build the release version"
	@echo "  make all            - Same as 'make'"
	@echo "  make debug          - Build with debug symbols"
	@echo "  make clean          - Remove all build artifacts"
	@echo "  make run            - Build and run the program"
	@echo "  make install        - Install to /usr/local/bin (Unix/Linux only)"
	@echo "  make uninstall      - Uninstall from /usr/local/bin (Unix/Linux only)"
	@echo "  make help           - Show this help message"
	@echo ""
	@echo "Variables you can override:"
	@echo "  CXX                 - C++ compiler (default: g++)"
	@echo "  CXXFLAGS            - Compiler flags"
	@echo ""
	@echo "Examples:"
	@echo "  make CXX=clang++    - Build with clang instead of g++"
	@echo "  make CXXFLAGS='-std=c++20 -O3'"

# Rebuild everything from scratch
.PHONY: rebuild
rebuild: clean all

# Dependencies (auto-generated)
-include $(OBJECTS:.o=.d)

# Generate dependency files
$(OBJ_DIR)$(PATH_SEP)%.d: $(SRC_DIR)$(PATH_SEP)%.cpp | $(OBJ_DIR)
	@$(CXX) $(CXXFLAGS) -MM -MT $(patsubst %.d,%.o,$@) $< > $@

.PHONY: deps
deps: $(patsubst %.cpp,$(OBJ_DIR)$(PATH_SEP)%.d,$(SOURCES))
