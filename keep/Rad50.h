#pragma once

#include <string>
#include <cstdint>

// RAD50 encoding/decoding for RSX-11M filenames

// RAD50 character set
extern const char rad50chars[];

// Decode one RAD50 word (3 characters)
std::string decodeRad50Word(uint16_t word);

// Decode RAD50 string from multiple words
std::string decodeRad50(const uint16_t* words, int count);

// Encode one RAD50 word from 3 characters
uint16_t encodeRad50Word(const std::string& str, int offset);

// Encode RAD50 string into multiple words
void encodeRad50(const std::string& str, uint16_t* words, int count);
