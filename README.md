# Fat32Tree

Fat32Tree is a cross-platform C++20 console application for reading
and displaying the file tree of a FAT32 partition image.

The project is designed to run on macOS, Linux, and Windows and relies
on the C++ standard library for platform-independent filesystem image access.

The parser is designed to tolerate filesystem corruption where possible,
preserve readable data, and continue processing unaffected parts of the image.

## Requirements

- C++20-compatible compiler
- CMake 3.20+
- Internet access during the initial CMake configuration to fetch GoogleTest

## Features

- FAT32 boot sector and BPB parsing
- FAT32 volume validation
- FAT and data-region layout calculation
- FAT cluster-chain traversal
- Support for fragmented cluster chains
- FAT 8.3 short file names
- Directory hierarchy traversal
- Iterative file-tree construction and output
- FAT cluster-chain cycle detection
- Logical directory cycle protection
- Cross-linked directory detection
- Partial-data preservation on recoverable corruption
- Diagnostic logging
- Cross-platform ASCII console output
- Unit and integration tests with GoogleTest / GoogleMock

## How It Works

The application processes a filesystem image through the following pipeline:

```text
Disk image
    ↓
Binary reader
    ↓
Boot sector / BPB parser
    ↓
FAT32 volume validation
    ↓
Derived volume layout
    ↓
FAT reader
    ↓
Cluster-chain traversal
    ↓
Directory-entry parsing
    ↓
Directory traversal
    ↓
File-tree construction
    ↓
Console output
```

The main processing flow is:

1. Open the supplied image file.
2. Parse the FAT32 boot sector and BPB.
3. Validate the filesystem parameters.
4. Derive FAT and data-region offsets and cluster information.
5. Follow FAT chains for directories.
6. Parse 32-byte FAT directory entries.
7. Ignore unsupported or non-user entries such as LFN and deleted entries.
8. Build the logical directory tree.
9. Detect repeated directory clusters to prevent cycles and repeated traversal.
10. Print the resulting tree to the console.

## Supported / Not Supported

| Area | Behavior |
|---|---|
| Filesystem type | FAT32 |
| Boot sector location | Expected at image offset `0` |
| Partition tables | Not parsed |
| File names | FAT 8.3 short names |
| Long File Names (LFN) | Ignored |
| Deleted directory entries | Skipped |
| Volume label | Used as the root display name when available |
| Directory traversal | Supported |
| Fragmented FAT chains | Supported |
| FAT cluster-chain cycles | Detected and stopped safely |
| Logical directory cycles | Detected using visited directory clusters |
| Cross-linked directories | Repeated directory clusters are detected; the node is preserved but duplicate traversal is skipped |
| Zero-size files | Supported |
| File content extraction | Not implemented |

## Fault Tolerance

The application separates fatal filesystem errors from recoverable local corruption.

| Situation | Behavior |
|---|---|
| Image cannot be opened | Fatal error; application exits |
| Invalid or unsupported FAT32 boot sector | Fatal error; application exits |
| Invalid FAT32 volume layout | Fatal error; application exits |
| Invalid directory entry | Entry is skipped and directory scanning continues |
| Deleted entry | Skipped |
| LFN entry | Ignored |
| Volume-label entry | Used for the root name when applicable |
| Directory end marker | Stops scanning the current logical directory |
| Free cluster encountered in a chain | Chain traversal stops; readable prefix is preserved |
| Bad cluster encountered in a chain | Chain traversal stops; readable prefix is preserved |
| Reserved cluster value | Chain traversal stops; readable prefix is preserved |
| Invalid cluster number | Chain traversal stops; readable prefix is preserved |
| FAT read failure | Chain traversal stops; readable prefix is preserved |
| FAT cluster-chain cycle | Cycle is detected; traversal stops without hanging |
| Repeated directory cluster | Directory node is preserved but the same directory is not traversed again |
| Cross-linked directory | Repeated traversal is skipped using the visited-directory set |
| Truncated directory data | Already readable entries are preserved; unreadable entries are treated as invalid |

Recoverable corruption is normally reported as `WARNING` while processing continues where possible.

Fatal failures are reported as `ERROR`.

## Build

### macOS / Linux

Configure:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

### Windows

Configure:

```powershell
cmake -S . -B build
```

Build the Release configuration:

```powershell
cmake --build build --config Release
```

Run tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

For Visual Studio multi-config generators, the executable is typically located at:

```text
build/Release/fat32tree.exe
```

The Windows build and test suite were also verified through GitHub Actions
using a GitHub-hosted Windows runner and MSVC.

## Run

Usage:

```text
fat32tree <image-path>
```

macOS / Linux example:

```bash
./fat32tree fat32_valid.img
```

Windows example:

```powershell
.\fat32tree.exe "C:\path\to\fat32_valid.img"
```

Paths containing spaces must be quoted by the shell.

Example output:

```text
TESTVOL/
|-- PHOTOS/
|   |-- TRIP/
|   |   `-- B.JPG
|   `-- A.JPG
|-- EMPTYDIR/
`-- README.TXT
```

ASCII tree characters are used instead of Unicode box-drawing characters
to keep console output consistent across macOS, Linux, and Windows terminals.

## Testing

The project includes both unit and integration tests covering the main
parsing, validation, FAT-chain, directory-processing, and tree-building components.

In addition to automated tests, the application was validated against real FAT32 images.

Real-image scenarios included:

- normal nested directory tree
- empty FAT32 volume
- zero-size files
- empty directory
- directory spanning multiple clusters
- 50 files in one directory
- intentionally corrupted FAT chain
- intentionally invalid directory entry
- nonexistent image
- malformed non-FAT image
- missing command-line argument

Cross-platform validation was performed using the same filesystem images
on macOS and Windows.

## Architecture

The main architectural separation is:

```text
raw binary access
    ↓
filesystem metadata parsing
    ↓
filesystem validation
    ↓
FAT-chain traversal
    ↓
directory parsing
    ↓
logical tree construction
    ↓
presentation
```

Tree traversal is iterative rather than recursive to avoid relying on
the process call stack for deeply nested directory structures.

## Logging

The application writes diagnostic logs to a timestamped log file on the user's Desktop.

Supported levels:

- `INFO`
- `DEBUG`
- `WARNING`
- `ERROR`

Recoverable corruption is normally reported as `WARNING` while processing
continues where possible.

Fatal failures are reported as `ERROR`.

## Limitations and Future Improvements

### Long File Names

The current implementation intentionally processes only FAT 8.3 short names.
LFN entries are ignored.

A future version could reconstruct LFN sequences and expose long file names.

### Partition Tables

The parser expects the FAT32 boot sector at image offset `0`.

MBR and GPT partition-table discovery are outside the current scope.

### Multiple FAT Copies

The current implementation uses the selected FAT determined from the parsed
volume layout.

More advanced recovery logic could compare or fall back to alternate FAT copies
if the selected FAT is damaged.

### Directory Read Failure Classification

Currently, `DirectoryEntryParser` returns the same `Invalid` state for both:

- a directory entry that is physically readable but semantically malformed
- a directory entry that cannot be fully read because the image is truncated

`DirectoryReader` therefore skips the current slot and continues scanning
the remaining slots in the current cluster.

This behavior is safe and preserves already readable entries, but after physical
EOF it may cause unnecessary repeated read attempts and repeated warnings for
the remaining slots of that cluster.

A future improvement would distinguish semantic corruption from physical read failure.

For a malformed but readable entry:

```text
warning
→ skip the current slot
→ continue scanning the same cluster
```

For physical EOF or a truncated slot:

```text
single warning
→ skip the remaining slots of the current cluster
→ continue with the next cluster in the FAT chain
→ preserve all entries successfully read before the failure
```

The whole logical directory should not be terminated solely because one physical
cluster reaches EOF. A fragmented FAT chain may reference a later logical cluster
whose physical offset is lower and still readable.
