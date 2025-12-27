#include "Rad50.h"
#include <cctype>

// RAD50 character set
const char rad50chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789";

// Decode one RAD50 word (3 characters)
std::string decodeRad50Word(uint16_t word) {
    if (word == 0) return "   ";
    std::string result;
    result += rad50chars[word / 1600];
    result += rad50chars[(word / 40) % 40];
    result += rad50chars[word % 40];
    return result;
}

// Decode RAD50 string from multiple words
std::string decodeRad50(const uint16_t* words, int count) {
    std::string result;
    for (int i = 0; i < count; i++) {
        result += decodeRad50Word(words[i]);
    }
    // Trim trailing spaces
    size_t end = result.find_last_not_of(' ');
    if (end != std::string::npos)
        result = result.substr(0, end + 1);
    return result;
}

// Encode one RAD50 word from 3 characters
uint16_t encodeRad50Word(const std::string& str, int offset) {
    uint16_t result = 0;
    for (int i = 0; i < 3; i++) {
        char c = (offset + i < str.length()) ? toupper(str[offset + i]) : ' ';
        int val = 0;
        
        // Find character in RAD50 table
        for (int j = 0; j < 40; j++) {
            if (rad50chars[j] == c) {
                val = j;
                break;
            }
        }
        result = result * 40 + val;
    }
    return result;
}

// Encode RAD50 string into multiple words
void encodeRad50(const std::string& str, uint16_t* words, int count) {
    for (int i = 0; i < count; i++) {
        words[i] = encodeRad50Word(str, i * 3);
    }
}
