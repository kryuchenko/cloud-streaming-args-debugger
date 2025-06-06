# Cross-platform development Makefile

# Detect OS
UNAME_S := $(shell uname -s)

# Default target
.PHONY: all
all: validate

# Platform-specific setup
.PHONY: setup
setup:
ifeq ($(UNAME_S),Darwin)
	@echo "🍎 Setting up for macOS..."
	./setup-dev-tools-mac.sh
else ifeq ($(OS),Windows_NT)
	@echo "🪟 Setting up for Windows..."
	setup-dev-tools.bat
else
	@echo "🐧 Setting up for Linux..."
	sudo apt-get update && sudo apt-get install -y clang clang-format clang-tidy cppcheck
	pip3 install pre-commit
endif

# Syntax validation
.PHONY: validate
validate:
ifeq ($(UNAME_S),Darwin)
	./validate.sh
else
	validate.bat
endif

# Format code
.PHONY: format
format:
	@echo "🎨 Formatting code..."
	clang-format -i *.cpp *.hpp tests/*.cpp

# Check formatting
.PHONY: format-check
format-check:
	@echo "🔍 Checking code formatting..."
	clang-format --dry-run --Werror *.cpp *.hpp tests/*.cpp

# Run static analysis
.PHONY: analyze
analyze:
	@echo "🔬 Running static analysis..."
	cppcheck --enable=warning,style,performance,portability --std=c++20 *.cpp

# Run all checks
.PHONY: check
check: validate format-check analyze
	@echo "✅ All checks completed!"

# Clean generated files
.PHONY: clean
clean:
	rm -f *.obj *.exe *.pdb
	rm -rf build/

# Help target
.PHONY: help
help:
	@echo "📚 Available targets:"
	@echo "  setup        - Install development tools"
	@echo "  validate     - Check syntax"
	@echo "  format       - Format code"
	@echo "  format-check - Check if code is formatted"
	@echo "  analyze      - Run static analysis"
	@echo "  check        - Run all checks"
	@echo "  clean        - Clean generated files"
	@echo "  help         - Show this help"