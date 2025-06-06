#!/bin/bash

echo "🛠️  Setting up development tools for macOS..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "📦 Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install development tools
echo "📦 Installing LLVM (includes clang-format, clang-tidy)..."
brew install llvm

# Add LLVM to PATH if not already there
if ! command -v clang-format &> /dev/null; then
    echo "🔧 Adding LLVM to PATH..."
    echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.zshrc
    echo 'export PATH="/opt/homebrew/opt/llvm/bin:$PATH"' >> ~/.bash_profile
    export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
fi

# Install Python and pip if needed
if ! command -v pip3 &> /dev/null; then
    echo "🐍 Installing Python..."
    brew install python
fi

# Install pre-commit
echo "🪝 Installing pre-commit..."
pip3 install pre-commit

# Install cppcheck for additional analysis
echo "🔍 Installing cppcheck..."
brew install cppcheck

# Setup pre-commit hooks
if [ -f ".pre-commit-config.yaml" ]; then
    echo "🔗 Setting up pre-commit hooks..."
    pre-commit install
fi

# Make validate script executable
chmod +x validate.sh

echo ""
echo "✅ Development tools setup complete!"
echo ""
echo "📋 Available commands:"
echo "   ./validate.sh           - Run syntax validation"
echo "   clang-format -i *.cpp   - Format code"
echo "   pre-commit run --all    - Run all checks"
echo "   cppcheck *.cpp          - Static analysis"
echo ""
echo "🔧 VS Code extensions recommended:"
echo "   - C/C++ (Microsoft)"
echo "   - clangd"
echo "   - Clang-Format"
echo ""
echo "💡 Restart your terminal or run: source ~/.zshrc"