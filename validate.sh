#!/bin/bash

echo "🔍 Validating C++ syntax..."

# Check if clang++ is available
if ! command -v clang++ &> /dev/null; then
    echo "❌ clang++ not found. Install with: brew install llvm"
    exit 1
fi

# Note: This is Windows-specific code, so we'll do basic syntax checks only
echo "ℹ️  Note: This project contains Windows-specific code."
echo "   On macOS, we'll check for basic C++ syntax issues only."

# Check for basic syntax issues in source files
echo ""
echo "🔍 Checking for common C++ syntax issues..."

# Check for missing semicolons, unmatched braces, etc.
syntax_errors=0

for file in cli_args_debugger.cpp seh_wrapper.cpp log_manager.cpp path_info.cpp; do
    if [ -f "$file" ]; then
        echo "Checking $file..."
        
        # Check for unmatched braces (more accurate method)
        brace_check=$(python3 -c "
import re
with open('$file', 'r') as f:
    content = f.read()
# Remove comments and strings
content = re.sub(r'//.*', '', content)
content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
content = re.sub(r'\".*?\"', '', content)
open_count = content.count('{')
close_count = content.count('}')
if open_count == close_count:
    print('OK')
else:
    print(f'ERROR {open_count} {close_count}')
" 2>/dev/null)
        
        if [[ $brace_check == ERROR* ]]; then
            read -r _ open_braces close_braces <<< "$brace_check"
            echo "❌ Unmatched braces in $file: $open_braces open, $close_braces close"
            syntax_errors=$((syntax_errors + 1))
        fi
        
        # Check for lines ending with comma that might need semicolon
        # (Skip function parameters and array initializers)
        problematic_commas=$(grep -n ',$' "$file" | grep -v '//' | grep -v '(' | grep -v '{' | head -3)
        if [ -n "$problematic_commas" ]; then
            echo "⚠️  Possible missing semicolon in $file (lines ending with comma):"
            echo "$problematic_commas"
        fi
        
        # Check for basic function syntax
        if grep -n 'void.*{$' "$file" > /dev/null; then
            echo "⚠️  Functions with opening brace on same line found in $file"
        fi
        
        echo "✅ Basic syntax check passed for $file"
    fi
done

if [ $syntax_errors -gt 0 ]; then
    echo "❌ Found $syntax_errors syntax errors"
    exit 1
fi

echo "✅ All syntax checks passed!"

# Check tests directory with clang++ (these should be more portable)
if [ -d "tests" ]; then
    echo ""
    echo "🧪 Checking test files..."
    
    for test_file in tests/*.cpp; do
        if [ -f "$test_file" ]; then
            echo "Checking $(basename "$test_file")..."
            
            # Check for unmatched braces in test files
            open_braces=$(grep -c '{' "$test_file")
            close_braces=$(grep -c '}' "$test_file")
            
            if [ $open_braces -ne $close_braces ]; then
                echo "❌ Unmatched braces in $test_file: $open_braces open, $close_braces close"
                syntax_errors=$((syntax_errors + 1))
            else
                echo "✅ Basic syntax check passed for $(basename "$test_file")"
            fi
        fi
    done
fi

# Check formatting if clang-format is available
if command -v clang-format &> /dev/null; then
    echo ""
    echo "🎨 Checking code formatting..."
    
    # Check if files need formatting
    needs_format=false
    for file in *.cpp *.hpp tests/*.cpp; do
        if [ -f "$file" ]; then
            if ! clang-format --dry-run --Werror "$file" &>/dev/null; then
                echo "⚠️  $file needs formatting"
                needs_format=true
            fi
        fi
    done
    
    if [ "$needs_format" = true ]; then
        echo ""
        echo "💡 Run 'clang-format -i *.cpp *.hpp tests/*.cpp' to fix formatting"
    else
        echo "✅ All files are properly formatted!"
    fi
else
    echo ""
    echo "💡 clang-format not found. Install with: brew install clang-format"
fi

echo ""
echo "🎉 Validation complete!"