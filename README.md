# Custom File System

## Overview
This project is a custom file system implementation using FUSE (Filesystem in Userspace). It was developed as part of the CS3650 Computer Systems course at Northeastern University. The file system supports basic functionality, including creating, reading, writing, renaming, and deleting files, as well as working with nested directories and handling large files.

## Features
- Create, read, write, rename, and delete files
- Support for file names with at least 10 characters
- Manage nested directories (mkdir, rmdir, readdir)
- Handle large files up to 500KB
- Efficient block management within a 1MB disk image

## Getting Started

### Prerequisites
- A Linux-based system or virtual machine
- The following packages installed:
  ```
  sudo apt-get install libfuse-dev libbsd-dev pkg-config
  ```

### Clone the Repository
```
git clone <repository-url>
cd custom-file-system
```

### Build and Run
Use the provided `Makefile` to simplify development:

- **Compile the program**:
  ```
  make nufs
  ```
- **Mount the filesystem**:
  ```
  make mount
  ```
- **Unmount the filesystem**:
  ```
  make unmount
  ```
- **Running the tests**:

You might need install an additional package to run the provided tests:
```
$ sudo apt-get install libtest-simple-perl
```
Then using 'make test' will run the provided tests.

- **Debug with GDB**:
  ```
  make gdb
  ```

## Usage
Once the filesystem is mounted, you can interact with it using standard file operations like `ls`, `touch`, `cat`, and `rm`. For example:
```
touch testfile
ls
cat testfile
rm testfile
```

## Directory Structure
- `nufs.c`: Main implementation of the file system
- `helpers/`: Utility functions for block and bitmap management
- `hints/`: Additional header files for inspiration
- `tests/`: Test cases to validate functionality
