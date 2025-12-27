# RSX Files-11 Filesystem Tool (rsxlst)

A command-line utility for reading and writing files to/from RSX-11M+ Files-11 ODS-1 disk images.

## Overview

`rsxlst` is a cross-platform tool that allows you to:
- List directory contents on RSX-11M+ disk images
- Copy files from RSX-11M+ disks to your host filesystem
- Copy files from your host filesystem to RSX-11M+ disks
- Display file headers and disk volume information
- Dump file header information for debugging

This tool supports the Files-11 ODS-1 (On-Disk Structure Level 1) format used by RSX-11M+ operating systems.

## Features

- **Cross-platform**: Works on Windows, Linux, and Unix systems
- **RAD-50 encoding support**: Properly handles RSX-11M+ filename encoding
- **Y2K date handling**: Correctly processes dates with Mentec's Y2K overflow encoding
- **Directory navigation**: Browse Files-11 directory structures with UIC (User Identification Code) support
- **File transfer**: Bidirectional file copying between host and RSX disk images
- **Debug capabilities**: Dump file headers and map areas for troubleshooting

## Building

### Prerequisites

- C++17 compatible compiler (GCC, Clang, or MSVC)
- Make (GNU Make or compatible)

### Compilation

#### Using Make (Linux/Unix/Windows with MinGW):

```bash
make
```

#### Build with debug symbols:

```bash
make debug
```

#### Clean build artifacts:

```bash
make clean
```

#### Rebuild from scratch:

```bash
make rebuild
```

#### Using Visual Studio (Windows):

Open `RsxLst.sln` and build the solution using Visual Studio 2019 or later.

## Installation

### Linux/Unix:

```bash
sudo make install
```

This installs the binary to `/usr/local/bin/rsxlst`.

### Windows:

Copy the built executable from `bin/rsxlst.exe` to a directory in your PATH.

## Usage

```
rsxlst <disk-image> <command> [options]
```

### Commands

#### List Directory Contents

```bash
rsxlst disk.dsk dir <group>,<member>
```

Lists all files in the specified directory. UIC (User Identification Code) is specified as group and member numbers in octal.

**Example:**
```bash
rsxlst sat.dsk dir 1,1
```

#### Copy File from RSX Disk to Host

```bash
rsxlst disk.dsk copyfrom <group>,<member> <filename>.<type> <output-file>
```

Copies a file from the RSX disk to your local filesystem.

**Example:**
```bash
rsxlst sat.dsk copyfrom 1,1 dfx.tsk dfx.tsk
```

**With specific version:**
```bash
rsxlst sat.dsk copyfrom 1,1 dfx.tsk;2 dfx_v2.tsk
```

#### Copy File from Host to RSX Disk

```bash
rsxlst disk.dsk copyto <group>,<member> <local-file> <filename>.<type>
```

Copies a file from your local filesystem to the RSX disk.

**Example:**
```bash
rsxlst sat.dsk copyto 1,1 myfile.bin newfile.tsk
```

**With version number:**
```bash
rsxlst sat.dsk copyto 1,1 myfile.bin newfile.tsk;2
```

#### Display Volume Information

```bash
rsxlst disk.dsk info
```

Shows disk volume information including volume name, structure level, and file limits.

#### Dump File Header

```bash
rsxlst disk.dsk dumpheader <file-number>
```

Displays detailed information about a file header for debugging purposes.

**Example:**
```bash
rsxlst sat.dsk dumpheader 7
```

#### Dump File Map Area

```bash
rsxlst disk.dsk dumpmap <file-number>
```

Shows the complete file header including retrieval pointers and map area.

**Example:**
```bash
rsxlst sat.dsk dumpmap 7
```

## RSX-11M+ Files-11 Format

### UIC (User Identification Code)

RSX-11M+ uses UICs to organize files into directories. A UIC consists of:
- **Group**: Octal number (e.g., 1, 200, 377)
- **Member**: Octal number (e.g., 1, 54, 177)

Format: `[group,member]` in octal notation

**Examples:**
- `[1,1]` - System directory
- `[200,1]` - User directory (group 200, member 1)

### File Naming Convention

Files in RSX-11M+ follow this format:
```
filename.type;version
```

- **Filename**: Up to 9 characters (RAD-50 encoded)
- **Type**: Up to 3 characters (file extension)
- **Version**: Integer version number (1-32767)

**Examples:**
- `HELLO.MAC;1` - Macro assembly source file, version 1
- `TEST.TSK;3` - Task image file, version 3

### Supported File Types

Common RSX-11M+ file types:
- `.TSK` - Task image (executable)
- `.MAC` - Macro-11 assembly source
- `.OBJ` - Object file
- `.TXT` - Text file
- `.DAT` - Data file
- `.DIR` - Directory file
- `.SYS` - System file

### Date Encoding (Y2K Format)

RSX-11M+ uses a special date encoding for years after 1999:
- Years since 1900 are encoded as two "digit" characters
- For 2000+, this causes overflow into non-digit ASCII characters
- Example: Year 2025 (offset 125) ? `<5` (where `<` = ASCII 60)

Format: `DDMMM<YHHMMSS` (13 bytes)
- `DD` - Day (01-31)
- `MMM` - Month (JAN, FEB, MAR, etc.)
- `<Y` - Year with overflow encoding
- `HHMMSS` - Time (hours, minutes, seconds)

## Examples

### List files in system directory [1,1]

```bash
rsxlst /mnt/rsx/system.dsk dir 1,1
```

Output:
```
=== Directory [001,001] ===
================================================================================
Filename        Type    Version File#  Blocks     Size        Date            Owner
--------------------------------------------------------------------------------
SYSTEM          TSK     1       10     50         25K         23-DEC-25 12:02 [001,001]
HELLO           MAC     2       15     10         5K          24-DEC-25 09:15 [001,001]
TEST            OBJ     1       20     5          2K          25-DEC-25 14:30 [001,001]
--------------------------------------------------------------------------------
Total files: 3  Total blocks: 65  Total size: 33280 bytes (32K)
```

### Copy a file from RSX disk

```bash
# Copy latest version
rsxlst system.dsk copyfrom 1,1 system.tsk system_backup.tsk

# Copy specific version
rsxlst system.dsk copyfrom 1,1 hello.mac;2 hello_v2.mac
```

### Copy a file to RSX disk

```bash
# Copy with automatic version 1
rsxlst system.dsk copyto 1,1 newprog.bin newprog.tsk

# Copy with specific version
rsxlst system.dsk copyto 1,1 update.dat update.dat;5
```

### Display disk information

```bash
rsxlst system.dsk info
```

Output:
```
Files-11 Volume: SYSTEM
Structure Level: 257 (ODS-1)

=== Volume Information ===
================================================================================
Volume Name: SYSTEM
Structure Level: 257 (ODS-1)
Max Files: 255
Index Bitmap LBN: 100
Index Bitmap Size: 10 blocks
```

### Debug file structure

```bash
# Show file header details
rsxlst system.dsk dumpheader 10

# Show complete header with map area
rsxlst system.dsk dumpmap 10
```

## File Structure

```
RsxLst/
??? RsxLst.cpp              # Main program entry point
??? Files11FileSystem.cpp   # Files-11 filesystem operations
??? Files11FileSystem.h     # Filesystem class definition
??? Rad50.cpp               # RAD-50 encoding/decoding
??? Rad50.h                 # RAD-50 header
??? DiskIO.cpp              # Low-level disk I/O operations
??? DiskIO.h                # Disk I/O header
??? FileStructures.h        # Files-11 data structures
??? Makefile                # Build configuration
??? README.md               # This file
```

## Architecture

### Core Components

1. **Files11FileSystem**: Main filesystem class
   - Directory navigation
   - File reading/writing
   - Header manipulation
   - Block allocation

2. **RAD-50 Encoder/Decoder**: Handles RSX filename encoding
   - Converts between ASCII and RAD-50
   - 3 characters per 16-bit word
   - Limited character set (A-Z, 0-9, space, $, .)

3. **DiskIO**: Low-level disk operations
   - Block-level read/write (512-byte blocks)
   - Byte order handling
   - File positioning

4. **FileStructures**: Data structure definitions
   - Home block structure
   - File header format
   - Directory entry format
   - UFAT (User File Attribute Table)

### Key Concepts

#### Files-11 ODS-1 Layout

```
+-------------------+
| Home Block        | LBN 1
+-------------------+
| Index File Bitmap | Variable location
+-------------------+
| File Headers      | Sequential allocation
+-------------------+
| Data Blocks       | Scattered across disk
+-------------------+
```

#### File Header Structure

- **Fixed area**: File number, sequence, owner UIC, protection
- **UFAT**: User File Attribute Table (32 bytes)
  - File size in blocks
  - Allocated blocks
  - Date/time stamps
- **Identification area**: Filename, type, version, dates
- **Map area**: Retrieval pointers (LBN and block count pairs)

#### Retrieval Pointers

Files-11 uses retrieval pointers to track file data locations:
- **LBN (Logical Block Number)**: Starting disk block
- **Count**: Number of consecutive blocks
- **Format**: Non-standard on some systems (e.g., `[lbn_hi][count][lbn_mid][lbn_lo]`)

## Limitations

- **Read-only for system files**: System files (MFD, BITMAP) cannot be modified
- **No file deletion**: Delete operation not implemented for safety
- **No directory creation**: Cannot create new directories
- **Block allocation**: Simple sequential search; may be slow on fragmented disks
- **Extension headers**: Limited support for files with extension headers
- **ODS-1 only**: Does not support ODS-2 (Files-11 Level 2) format

## Troubleshooting

### "Cannot open disk image"
- Verify the disk image file exists and path is correct
- Check file permissions (read access for `dir`/`copyfrom`, write access for `copyto`)

### "Directory not found"
- Ensure UIC is specified in octal (e.g., `1,1` not `1,0x1`)
- Verify the directory exists using `dir` on parent directory

### "File not found"
- Check filename spelling and case (RSX uses uppercase)
- Verify file type extension (e.g., `.TSK` not `.TSK`)
- Confirm version number if specified

### "Cannot allocate enough blocks"
- Disk may be full or heavily fragmented
- Try a different disk image with more free space

### Date shows incorrectly
- Post-1999 dates use overflow encoding (`<`, `=`, `>`, etc.)
- This is normal RSX-11M+ Y2K behavior
- The tool correctly decodes these dates

## Technical Details

### RAD-50 Encoding

RAD-50 packs 3 characters into 16 bits:
```
value = (c1 * 40 * 40) + (c2 * 40) + c3
```

Character set (40 characters):
```
0: space    1-26: A-Z    27-36: 0-9    37: $    38: .    39: (unused)
```

### Y2K Date Encoding

Year encoding formula:
```
tens_digit = (year_offset / 10) + '0'   # ASCII character
units_digit = (year_offset % 10) + '0'  # ASCII character
```

For year 2025 (offset 125 from 1900):
```
tens = 125 / 10 = 12 ? 12 + 48 = 60 ? '<' (ASCII 60)
units = 125 % 10 = 5 ? 5 + 48 = 53 ? '5' (ASCII 53)
Result: "<5"
```

### Block Size

Files-11 ODS-1 uses 512-byte blocks:
- All I/O operations are block-aligned
- Partial blocks are padded with zeros
- File size stored separately from allocated blocks

## Contributing

Contributions are welcome! Areas for improvement:
- ODS-2 (Files-11 Level 2) support
- File deletion capability
- Directory creation
- Better error handling and recovery
- Performance optimization for large disks
- Support for more RSX variants

## License

This tool is provided as-is for educational and archival purposes.

## References

- DEC RSX-11M+ Documentation
- Files-11 On-Disk Structure Specification
- RSX-11M-PLUS System Internals Manual
- Mentec Y2K Updates for RSX-11M+

## Version History

- **v1.0** - Initial release
  - Directory listing
  - File copying (both directions)
  - RAD-50 encoding support
  - Y2K date handling
  - Basic debug commands

## Contact & Support

For issues, questions, or contributions, please refer to the project repository.

---

*Note: This tool is designed for working with legacy RSX-11M+ disk images and should not be used with production systems without proper backups.*
