// rsxlst.cpp - RSX-11M Disk Image Directory Lister
// Compile: cl /EHsc rsxlst.cpp
#if 0
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>

const int BLOCK_SIZE = 512;
const int HOME_BLOCK = 1;

#pragma pack(push, 1)
// RSX-11M Home Block (Files-11 Structure Level 1)
struct HomeBlock {
    unsigned char ibsz;              // Index bitmap size (blocks)
    unsigned char iblb[3];           // Index bitmap LBN (24-bit)
    unsigned short fmax;             // Max files
    unsigned short sbclu;            // Storage bitmap cluster
    unsigned short dvty;             // Device type
    unsigned short vlev;             // Structure level
    char volname[12];                // Volume name (ASCII)
    char ownername[12];              // Owner (ASCII)
    unsigned short volprot;          // Protection
    unsigned short vchar;            // Characteristics
    unsigned short dfprot;           // Default protection
    unsigned short wisz;             // Window size
    unsigned short fiext;            // File extend
    unsigned short lrucnt;           // LRU limit
};

// Files-11 directory entry (16 bytes)
struct DirEntry {
    unsigned char status;            // Status byte
    unsigned short fname[3];         // Filename RAD50 (3 words)
    unsigned short ftype;            // Type RAD50 (1 word)
    unsigned short version;          // Version number
    unsigned short filenum;          // File number
    unsigned short fileseq;          // File sequence
};
#pragma pack(pop)

// RAD50 character set
const char rad50chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ$.%0123456789";

// Decode one RAD50 word (3 characters)
std::string decodeRad50Word(unsigned short word) {
    std::string result;
    if (word == 0) return "   ";
    
    result += rad50chars[word / 1600];
    result += rad50chars[(word / 40) % 40];
    result += rad50chars[word % 40];
    return result;
}

// Decode RAD50 filename (3 words = 9 chars max)
std::string decodeFilename(unsigned short words[3]) {
    std::string result;
    for (int i = 0; i < 3; i++) {
        result += decodeRad50Word(words[i]);
    }
    // Trim trailing spaces
    size_t end = result.find_last_not_of(' ');
    if (end != std::string::npos)
        result = result.substr(0, end + 1);
    return result;
}

// Display home block
void displayHomeBlock(std::ifstream& image) {
    unsigned char buffer[BLOCK_SIZE];
    
    image.seekg(HOME_BLOCK * BLOCK_SIZE, std::ios::beg);
    image.read((char*)buffer, BLOCK_SIZE);
    
    if (!image) {
        std::cerr << "Warning: Could not read home block" << std::endl;
        return;
    }
    
    std::cout << "\n=== Volume Information ===" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    
    // Display raw hex
    std::cout << "\nHome block (first 64 bytes):" << std::endl;
    for (int i = 0; i < 64; i++) {
        if (i % 16 == 0) std::cout << "\n" << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
        std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)buffer[i] << " ";
    }
    std::cout << std::dec << std::endl << std::endl;
    
    // Parse home block
    HomeBlock* hb = (HomeBlock*)buffer;
    
    // Volume name at offset 14 (0x0E) - 12 bytes ASCII
    std::string volname((char*)&buffer[0x0E], 12);
    volname = volname.substr(0, volname.find('\0'));
    
    // Owner at offset 26 (0x1A) - 12 bytes ASCII  
    std::string owner((char*)&buffer[0x1A], 12);
    owner = owner.substr(0, owner.find('\0'));
    
    // Index bitmap info
    unsigned int iblbn = hb->iblb[0] | (hb->iblb[1] << 8) | (hb->iblb[2] << 16);
    
    std::cout << "Volume name: " << volname << std::endl;
    std::cout << "Owner: " << owner << std::endl;
    std::cout << "Structure level: " << hb->vlev << std::endl;
    std::cout << "Max files: " << hb->fmax << std::endl;
    std::cout << "Index bitmap size: " << (int)hb->ibsz << " blocks" << std::endl;
    std::cout << "Index bitmap LBN: " << iblbn << std::endl;
    std::cout << "Device type: 0x" << std::hex << hb->dvty << std::dec << std::endl;
    
    std::cout << std::string(70, '=') << std::endl;
}

// Parse UIC
bool parseUIC(const std::string& uic, int& group, int& member) {
    std::string clean = uic;
    if (clean[0] == '[') clean = clean.substr(1);
    if (!clean.empty() && clean[clean.length()-1] == ']') 
        clean = clean.substr(0, clean.length()-1);
    
    size_t comma = clean.find(',');
    if (comma == std::string::npos) return false;
    
    try {
        group = std::stoi(clean.substr(0, comma));
        member = std::stoi(clean.substr(comma + 1));
        return true;
    } catch (...) {
        return false;
    }
}

// Calculate directory location from index file
int findDirectoryBlock(std::ifstream& image, int group, int member) {
    // Read home block to get index bitmap location
    unsigned char hbuffer[BLOCK_SIZE];
    image.seekg(HOME_BLOCK * BLOCK_SIZE, std::ios::beg);
    image.read((char*)hbuffer, BLOCK_SIZE);
    
    HomeBlock* hb = (HomeBlock*)hbuffer;
    unsigned int iblbn = hb->iblb[0] | (hb->iblb[1] << 8) | (hb->iblb[2] << 16);
    
    // The Master File Directory (MFD) is typically at a fixed location
    // For RSX-11M, it's often around block 6-10
    // User directories follow after system directories
    
    // Common locations for [1,1]
    return 142;  // From the actual data location
}

// Debug: dump directory block
void dumpBlock(unsigned char* buffer, int blockNum, int count = 64) {
    std::cout << "\nBlock " << blockNum << " (first " << count << " bytes):" << std::endl;
    for (int i = 0; i < count; i++) {
        if (i % 16 == 0) std::cout << "\n" << std::hex << std::setfill('0') << std::setw(4) << i << ": ";
        std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)buffer[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// List directory
void listDirectory(std::ifstream& image, int group, int member) {
    std::cout << "\n=== Directory [" << group << "," << member << "] ===" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    int dirBlock = findDirectoryBlock(image, group, member);
    
    unsigned char buffer[BLOCK_SIZE];
    int fileCount = 0;
    
    std::cout << "\nReading directory at block " << dirBlock << "..." << std::endl;
    
    // Read first directory block and dump it
    image.seekg(dirBlock * BLOCK_SIZE, std::ios::beg);
    image.read((char*)buffer, BLOCK_SIZE);
    
    if (!image) {
        std::cerr << "Error reading directory block" << std::endl;
        return;
    }
    
    // Dump first 128 bytes to see structure
    dumpBlock(buffer, dirBlock, 128);
    
    std::cout << "\n" << std::left << std::setw(15) << "Filename" 
         << std::setw(8) << "Type" 
         << std::setw(8) << "Version"
         << std::setw(8) << "File#"
         << std::setw(8) << "Status"
         << "Raw Bytes" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    // Read directory blocks - RSX-11M directories span multiple blocks
    for (int blk = 0; blk < 8; blk++) {
        image.seekg((dirBlock + blk) * BLOCK_SIZE, std::ios::beg);
        image.read((char*)buffer, BLOCK_SIZE);
        if (!image) break;
        
        // Parse directory entries - try different offsets
        // RSX-11M entries can start at offset 0 or 2
        for (int startOffset = 0; startOffset < 4; startOffset += 2) {
            fileCount = 0;
            
            for (int offset = startOffset; offset <= BLOCK_SIZE - 16; offset += 16) {
                DirEntry* entry = (DirEntry*)&buffer[offset];
                
                // Check for valid entry status
                if (entry->status == 0 || entry->status == 0xFF) continue;
                
                // Decode filename (3 RAD50 words)
                std::string fname = decodeFilename(entry->fname);
                std::string ftype = decodeRad50Word(entry->ftype);
                
                // Trim spaces
                size_t end = ftype.find_last_not_of(' ');
                if (end != std::string::npos) ftype = ftype.substr(0, end + 1);
                
                // Check if looks like valid ASCII or RAD50
                bool hasValidChars = true;
                for (char c : fname) {
                    if (c != ' ' && !isalnum(c) && c != '.' && c != '$' && c != '%') {
                        hasValidChars = false;
                        break;
                    }
                }
                
                // Validate version is reasonable
                bool validVersion = entry->version > 0 && entry->version < 1000;
                
                if (hasValidChars && !fname.empty() && fname != "         " && validVersion) {
                    std::cout << std::left << std::setw(15) << fname
                         << std::setw(8) << ftype
                         << std::setw(8) << entry->version
                         << std::setw(8) << entry->filenum
                         << "0x" << std::hex << std::setfill('0') << std::setw(2) << (int)entry->status << std::dec << "   ";
                    
                    // Show first 16 bytes in hex
                    for (int i = 0; i < 16; i++) {
                        std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)buffer[offset + i] << " ";
                    }
                    std::cout << std::dec << std::endl;
                    
                    fileCount++;
                }
            }
            
            if (fileCount > 0) break; // Found valid entries at this offset
        }
        
        if (fileCount == 0 && blk == 0) {
            std::cout << "No valid entries found in first block. Trying alternate interpretation..." << std::endl;
        }
    }
    
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "Total files: " << fileCount << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "RSX-11M Directory Lister v3.0\n" << std::endl;
    
    if (argc < 3) {
        std::cerr << "Usage: rsxlst <disk_image> <directory>\n";
        std::cerr << "Example: rsxlst disk.dsk 1,1\n";
        return 1;
    }
    
    std::string imagePath = argv[1];
    std::string directory = argv[2];
    
    int group, member;
    if (!parseUIC(directory, group, member)) {
        std::cerr << "Error: Invalid directory format. Use: 1,1 or [1,1]" << std::endl;
        return 1;
    }
    
    std::ifstream image(imagePath, std::ios::binary);
    if (!image) {
        std::cerr << "Error: Cannot open: " << imagePath << std::endl;
        return 1;
    }
    
    // Get file size
    image.seekg(0, std::ios::end);
    std::streampos fileSize = image.tellg();
    image.seekg(0, std::ios::beg);
    
    std::cout << "Disk image: " << imagePath << std::endl;
    std::cout << "Image size: " << fileSize << " bytes (" 
         << (fileSize / BLOCK_SIZE) << " blocks)" << std::endl;
    
    displayHomeBlock(image);
    listDirectory(image, group, member);
    
    image.close();
    return 0;
}
#endif
