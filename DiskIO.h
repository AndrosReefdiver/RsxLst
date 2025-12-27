#pragma once

#include <fstream>
#include <cstdint>

// Low-level disk I/O operations for Files-11 ODS-1

// Read a block from disk image
bool readBlock(std::ifstream& img, uint32_t lbn, uint8_t* buffer);

// Write a block to disk image
bool writeBlock(std::ofstream& img, uint32_t lbn, const uint8_t* buffer);

// Write a block to disk image (fstream overload for read-write access)
bool writeBlock(std::fstream& img, uint32_t lbn, const uint8_t* buffer);

// Get word from buffer (little-endian)
uint16_t getWord(const uint8_t* buf, int offset);

// Set word in buffer (little-endian)
void setWord(uint8_t* buf, int offset, uint16_t value);
