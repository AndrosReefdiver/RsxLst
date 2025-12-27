// rsxlst.cpp - RSX-11M Files-11 ODS-1 File System Reader
// Main entry point and command-line interface

#include "Files11FileSystem.h"
#include "FileStructures.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <iomanip>

// Display help information
void displayHelp() {
    std::cout << "RSX-11M Files-11 ODS-1 File System Tool - Help\n" << std::endl;
    std::cout << "This tool provides functions to read and interpret RSX-11M Files-11 ODS-1 disk images," << std::endl;
    std::cout << "focusing on INDEXF.SYS parsing, file headers, and directory reading." << std::endl;
    std::cout << "Use this tool to explore and extract files from RSX-11M disk images." << std::endl;
    
    std::cout << "\nAvailable commands:" << std::endl;
    std::cout << "  info                  - Display volume information" << std::endl;
    std::cout << "  dir <uic>             - List directory (e.g., 1,1 or [1,1])" << std::endl;
    std::cout << "  file <uic> <filename> - Show detailed info for one file" << std::endl;
    std::cout << "  header <filenum>      - Display file header details" << std::endl;
    std::cout << "  entry <uic> <file>    - Display directory entry & file header" << std::endl;
    std::cout << "  copyfrom <uic> <file> <dest> - Copy file from Files-11 to Windows" << std::endl;
    std::cout << "  copyto <uic> <src> <file>    - Copy file from Windows to Files-11" << std::endl;
    std::cout << "  extract <uic> <file>  - Extract file (legacy command)" << std::endl;
    std::cout << "  dumpmap <filenum>     - Dump raw map area for debugging" << std::endl;
    std::cout << "  help                  - Display this help information" << std::endl;
    
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  rsxlst mydisk.hd info                      - Show volume info" << std::endl;
    std::cout << "  rsxlst mydisk.hd dir 1,2                   - List directory [1,2]" << std::endl;
    std::cout << "  rsxlst mydisk.hd file 1,2 myfile.txt       - Show info for file" << std::endl;
    std::cout << "  rsxlst mydisk.hd header 4                  - Display header for file #4" << std::endl;
    std::cout << "  rsxlst mydisk.hd copyfrom 1,2 file.txt C:\\out.txt - Copy from Files-11" << std::endl;
    std::cout << "  rsxlst mydisk.hd copyto 1,2 C:\\src.txt file.txt   - Copy to Files-11" << std::endl;
    std::cout << "  rsxlst mydisk.hd dumpmap 4                 - Dump map area for file #4" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "RSX-11M Files-11 ODS-1 File System Tool v4.0\n" << std::endl;
    
    // Check for help request
    if (argc == 2) {
        std::string arg = argv[1];
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
        if (arg == "help" || arg == "/help" || arg == "-help" || arg == "--help" || 
            arg == "/h" || arg == "-h" || arg == "/?" || arg == "-?") {
            displayHelp();
            return 0;
        }
    }
    
    if (argc < 3) {
        std::cerr << "Usage: rsxlst <disk_image> <command> [options]\n";
        std::cerr << "       rsxlst help    - Display detailed help\n";
        std::cerr << "\nCommands:\n";
        std::cerr << "  info                          - Display volume information\n";
        std::cerr << "  dir <uic>                     - List directory (e.g., 1,1 or [1,1])\n";
        std::cerr << "  file <uic> <filename>         - Show detailed info for one file\n";
        std::cerr << "  header <filenum>              - Display file header details\n";
        std::cerr << "  entry <uic> <file>            - Display directory entry & file header\n";
        std::cerr << "  copyfrom <uic> <file> <dest>  - Copy file from Files-11 to Windows\n";
        std::cerr << "  copyto <uic> <src> <file>     - Copy file from Windows to Files-11\n";
        std::cerr << "  extract <uic> <file>          - Extract file (legacy command)\n";
        std::cerr << "  dumpmap <filenum>             - Dump raw map area for debugging\n";
        std::cerr << "\nFor detailed help: rsxlst help\n";
        return 1;
    }
    
    std::string imagePath = argv[1];
    std::string command = argv[2];
    
    // Check if command is help
    std::string cmdLower = command;
    std::transform(cmdLower.begin(), cmdLower.end(), cmdLower.begin(), ::tolower);
    if (cmdLower == "help" || cmdLower == "/help" || cmdLower == "-help" || 
        cmdLower == "/h" || cmdLower == "-h" || cmdLower == "/?" || cmdLower == "-?") {
        displayHelp();
        return 0;
    }
    
    Files11FileSystem fs(imagePath);
    if (!fs.initialize()) {
        return 1;
    }
    
    // Parse and execute command
    std::transform(command.begin(), command.end(), command.begin(), ::tolower);
    if (command == "info") {
        fs.displayHomeBlock();
    }
    else if (command == "dir" && argc == 4) {
        // Parse UIC from single argument like "1,1" or "[1,1]"
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format. Use: 1,1 or [1,1]" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        fs.listDirectory(group, member, false);
    }
    else if (command == "file" && argc == 5) {
        // Parse UIC
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        
        // Parse filename (NAME.TYPE or NAME.TYPE;version)
        std::string file = argv[4];
        std::string name, type;
        int version = -1;
        
        size_t dot = file.find('.');
        size_t semi = file.find(';');
        
        if (dot != std::string::npos) {
            name = file.substr(0, dot);
            if (semi != std::string::npos) {
                type = file.substr(dot + 1, semi - dot - 1);
                version = std::stoi(file.substr(semi + 1));
            } else {
                type = file.substr(dot + 1);
            }
        } else {
            name = file;
            type = "";
        }
        
        // Convert to uppercase
        for (auto& c : name) c = toupper(c);
        for (auto& c : type) c = toupper(c);
        
        fs.displayDirectoryEntry(group, member, name, type, version);
    }
    else if (command == "header" && argc == 4) {
        int fileNum = std::stoi(argv[3]);
        fs.displayFileHeader(fileNum);
    }
    else if (command == "entry" && argc == 5) {
        // Parse UIC
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        
        // Parse filename
        std::string file = argv[4];
        std::string name, type;
        int version = -1;
        
        size_t dot = file.find('.');
        size_t semi = file.find(';');
        
        if (dot != std::string::npos) {
            name = file.substr(0, dot);
            if (semi != std::string::npos) {
                type = file.substr(dot + 1, semi - dot - 1);
                version = std::stoi(file.substr(semi + 1));
            } else {
                type = file.substr(dot + 1);
            }
        } else {
            name = file;
            type = "";
        }
        
        // Convert to uppercase
        for (auto& c : name) c = toupper(c);
        for (auto& c : type) c = toupper(c);
        
        fs.displayDirectoryEntry(group, member, name, type, version);
    }
    else if (command == "copyfrom" && argc == 6) {
        // Parse UIC
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        
        // Parse filename
        std::string file = argv[4];
        std::string outputPath = argv[5];
        
        std::string name, type;
        int version = -1;
        
        size_t dot = file.find('.');
        size_t semi = file.find(';');
        
        if (dot != std::string::npos) {
            name = file.substr(0, dot);
            if (semi != std::string::npos) {
                type = file.substr(dot + 1, semi - dot - 1);
                version = std::stoi(file.substr(semi + 1));
            } else {
                type = file.substr(dot + 1);
            }
        } else {
            name = file;
            type = "";
        }
        
        // Convert to uppercase
        for (auto& c : name) c = toupper(c);
        for (auto& c : type) c = toupper(c);
        
        fs.copyFromFiles11(group, member, name, type, version, outputPath);
    }
    else if (command == "copyto" && argc == 6) {
        // Parse UIC
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        
        std::string localPath = argv[4];
        std::string destFile = argv[5];
        
        fs.copyToFiles11(group, member, localPath, destFile);
    }
    else if (command == "extract" && argc == 5) {
        // Parse UIC
        std::string uic = argv[3];
        if (!uic.empty() && uic[0] == '[') uic = uic.substr(1);
        if (!uic.empty() && uic[uic.length()-1] == ']') 
            uic = uic.substr(0, uic.length()-1);
        
        size_t comma = uic.find(',');
        if (comma == std::string::npos) {
            std::cerr << "Error: Invalid UIC format" << std::endl;
            return 1;
        }
        
        int group = std::stoi(uic.substr(0, comma));
        int member = std::stoi(uic.substr(comma + 1));
        
        // Parse filename
        std::string file = argv[4];
        std::string name, type;
        int version = -1;
        
        size_t dot = file.find('.');
        size_t semi = file.find(';');
        
        if (dot != std::string::npos) {
            name = file.substr(0, dot);
            if (semi != std::string::npos) {
                type = file.substr(dot + 1, semi - dot - 1);
                version = std::stoi(file.substr(semi + 1));
            } else {
                type = file.substr(dot + 1);
            }
        } else {
            name = file;
            type = "";
        }
        
        // Convert to uppercase
        for (auto& c : name) c = toupper(c);
        for (auto& c : type) c = toupper(c);
        
        std::string outputPath = "extracted_" + name + "." + type;
        fs.extractFile(group, member, name, type, version, outputPath);
    }
    else if (command == "dumpmap" && argc == 4) {
        int fileNum = std::stoi(argv[3]);
        
        FileHeader header;
        if (!fs.readFileHeader(fileNum, header)) {
            std::cerr << "Error: Cannot read file header" << std::endl;
            return 1;
        }
        
        const uint8_t* headerBytes = reinterpret_cast<const uint8_t*>(&header);
        int mapOffset = header.h_mpof * 2;
        const uint8_t* mapArea = headerBytes + mapOffset;
        int mapSize = BLOCK_SIZE - mapOffset;
        
        std::cout << "\n=== Map Area Dump for File #" << fileNum << " ===" << std::endl;
        std::cout << "Map offset: " << mapOffset << " bytes" << std::endl;
        std::cout << "Map size: " << mapSize << " bytes" << std::endl;
        
        // Dump entire header first
        std::cout << "\n=== Complete File Header (512 bytes) ===" << std::endl;
        for (int i = 0; i < BLOCK_SIZE; i++) {
            if (i % 16 == 0) {
                std::cout << std::hex << std::setfill('0') << std::setw(3) << i << ": ";
            }
            std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)headerBytes[i] << " ";
            if ((i + 1) % 16 == 0) {
                // Show ASCII interpretation
                std::cout << " | ";
                for (int j = i - 15; j <= i; j++) {
                    char c = headerBytes[j];
                    if (c >= 32 && c <= 126) {
                        std::cout << c;
                    } else {
                        std::cout << '.';
                    }
                }
                std::cout << std::endl;
            }
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "\n=== Header Fixed Fields ===" << std::endl;
        std::cout << "h_idof (ident offset): " << header.h_idof << " words (" << (header.h_idof * 2) << " bytes)" << std::endl;
        std::cout << "h_mpof (map offset): " << header.h_mpof << " words (" << (header.h_mpof * 2) << " bytes)" << std::endl;
        std::cout << "h_fnum (file number): " << header.h_fnum << std::endl;
        std::cout << "h_fseq (file sequence): " << header.h_fseq << std::endl;
        std::cout << "h_flev (structure level): " << header.h_flev << std::endl;
        std::cout << "h_fown (owner UIC): [" << std::oct << ((header.h_fown >> 8) & 0xFF) << "," 
                  << (header.h_fown & 0xFF) << "]" << std::dec << std::endl;
        std::cout << "h_fpro (protection): 0x" << std::hex << header.h_fpro << std::dec << std::endl;
        
        std::cout << "\nFirst 64 bytes of map area:" << std::endl;
        for (int i = 0; i < 64 && i < mapSize; i++) {
            if (i % 16 == 0) std::cout << std::hex << std::setfill('0') << std::setw(3) << i << ": ";
            std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)mapArea[i] << " ";
            if ((i + 1) % 16 == 0) std::cout << std::endl;
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "\nMap control fields:" << std::endl;
        std::cout << "M.CTSZ [6] = " << (int)mapArea[6] << " (count field size)" << std::endl;
        std::cout << "M.LBSZ [7] = " << (int)mapArea[7] << " (LBN field size)" << std::endl;
        std::cout << "M.USE  [8] = " << (int)mapArea[8] << " (map words in use)" << std::endl;
        
        std::cout << "\nRaw pointer bytes at offset 10:" << std::endl;
        std::cout << "  Bytes: ";
        for (int i = 10; i < 14 && i < mapSize; i++) {
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)mapArea[i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "\n=== UFAT area (bytes 14-45 of header) ===" << std::endl;
        for (int i = 0; i < 32; i++) {
            if (i % 16 == 0) std::cout << "  Offset " << std::dec << i << ": ";
            std::cout << std::hex << std::setfill('0') << std::setw(2) << (int)header.h_ufat[i] << " ";
            if ((i + 1) % 16 == 0) std::cout << std::endl;
        }
        std::cout << std::dec << std::endl;
        
        // Decode UFAT fields
        uint16_t ufatBlockCount = header.h_ufat[0] | (header.h_ufat[1] << 8);
        uint16_t ufatEOFByte = header.h_ufat[4] | (header.h_ufat[5] << 8);
        uint16_t ufatAllocatedBlocks = header.h_ufat[6] | (header.h_ufat[7] << 8);
        uint16_t ufatDate1 = header.h_ufat[12] | (header.h_ufat[13] << 8);
        uint16_t ufatTime1 = header.h_ufat[14] | (header.h_ufat[15] << 8);
        uint16_t ufatDate2 = header.h_ufat[20] | (header.h_ufat[21] << 8);
        uint16_t ufatTime2 = header.h_ufat[22] | (header.h_ufat[23] << 8);
        
        std::cout << "\n=== Decoded UFAT Fields ===" << std::endl;
        std::cout << "File size from UFAT[0-1]: " << ufatBlockCount << " blocks (0x" 
                  << std::hex << ufatBlockCount << std::dec << ")" << std::endl;
        std::cout << "EOF byte position UFAT[4-5]: " << ufatEOFByte << " bytes (0x" 
                  << std::hex << ufatEOFByte << std::dec << ")" << std::endl;
        std::cout << "Allocated blocks UFAT[6-7]: " << ufatAllocatedBlocks << " blocks (0x" 
                  << std::hex << ufatAllocatedBlocks << std::dec << ")" << std::endl;
        
        // Decode date/time from both possible locations
        if (ufatDate1 > 0) {
            int year = (ufatDate1 / 512) + 1900;
            int month = (ufatDate1 % 512) / 32;
            int day = ufatDate1 % 32;
            std::cout << "Date at UFAT[12-13]: " << year << "-" << month << "-" << day 
                      << " (raw: 0x" << std::hex << ufatDate1 << std::dec << ")" << std::endl;
        } else {
            std::cout << "Date at UFAT[12-13]: Not set (0x" << std::hex << ufatDate1 << std::dec << ")" << std::endl;
        }
        if (ufatTime1 > 0) {
            int hour = ufatTime1 / 2048;
            int minute = (ufatTime1 % 2048) / 32;
            int second = (ufatTime1 % 32) * 2;
            std::cout << "Time at UFAT[14-15]: " << hour << ":" << minute << ":" << second 
                      << " (raw: 0x" << std::hex << ufatTime1 << std::dec << ")" << std::endl;
        } else {
            std::cout << "Time at UFAT[14-15]: Not set (0x" << std::hex << ufatTime1 << std::dec << ")" << std::endl;
        }
        if (ufatDate2 > 0) {
            int year = (ufatDate2 / 512) + 1900;
            int month = (ufatDate2 % 512) / 32;
            int day = ufatDate2 % 32;
            std::cout << "Date at UFAT[20-21]: " << year << "-" << month << "-" << day 
                      << " (raw: 0x" << std::hex << ufatDate2 << std::dec << ")" << std::endl;
        } else {
            std::cout << "Date at UFAT[20-21]: Not set (0x" << std::hex << ufatDate2 << std::dec << ")" << std::endl;
        }
        if (ufatTime2 > 0) {
            int hour = ufatTime2 / 2048;
            int minute = (ufatTime2 % 2048) / 32;
            int second = (ufatTime2 % 32) * 2;
            std::cout << "Time at UFAT[22-23]: " << hour << ":" << minute << ":" << second 
                      << " (raw: 0x" << std::hex << ufatTime2 << std::dec << ")" << std::endl;
        } else {
            std::cout << "Time at UFAT[22-23]: Not set (0x" << std::hex << ufatTime2 << std::dec << ")" << std::endl;
        }

    }
    else {
        std::cerr << "Error: Unknown command or wrong number of arguments: " << command << std::endl;
        std::cerr << "Type 'rsxlst help' for list of available commands." << std::endl;
        return 1;
    }
    
    return 0;
}