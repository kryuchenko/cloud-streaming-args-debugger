#pragma once

#include <string>
#include <vector>

/**
 * Generate header text based on whether we have arguments or not.
 * 
 * @param args Array of command-line arguments
 * @return Text indicating whether arguments were received
 */
inline std::wstring BuildCliHeaderText(const std::vector<std::wstring>& args) {
    return args.empty() 
        ? L"No arguments were received." 
        : L"Received the following arguments:";
}

/**
 * Format command-line arguments according to Windows conventions:
 * - Arguments without spaces or quotes are not quoted
 * - Arguments with spaces or quotes are wrapped in double quotes
 * - Empty arguments are wrapped in double quotes
 * - No trailing space
 * 
 * @param args Array of command-line arguments
 * @return Formatted string of arguments
 */
inline std::wstring BuildCliArgsText(const std::vector<std::wstring>& args) {
    std::wstring result;
    for (size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        
        // Check if argument is empty, contains spaces or quotes and needs quotes
        bool needsQuotes = arg.empty() || arg.find(L' ') != std::wstring::npos || arg.find(L'"') != std::wstring::npos;
        
        if (needsQuotes) {
            result += L"\"" + arg + L"\"";
        } else {
            result += arg;
        }
        
        // Add space between arguments, but not after the last one
        if (i < args.size() - 1) {
            result += L" ";
        }
    }
    return result;
}