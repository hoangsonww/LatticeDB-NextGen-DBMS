# LatticeDB Build Instructions

## Prerequisites

### Required
- **C++17 compatible compiler** (GCC 8+, Clang 10+, MSVC 2019+)
- **CMake 3.15** or higher
- **Make** or **Ninja** build system
- Standard C++ libraries with threading support

### Optional (Automatically detected)
- **LZ4** compression library (for fast compression)
- **Zstandard (zstd)** compression library (for high compression ratio)
- **readline** library (for enhanced CLI experience)
- **pkg-config** (for finding dependencies)

## Supported Platforms

- ✅ **Linux** (Ubuntu 20.04+, Fedora 35+, RHEL 8+)
- ✅ **macOS** (11.0 Big Sur or later)
- ✅ **Windows** (WSL2 recommended, native with MSVC 2019+)
- ✅ **Docker** (Multi-platform containers)

## Quick Build

```bash
# Clone the repository
git clone https://github.com/hoangsonww/LatticeDB-DBMS.git
cd LatticeDB-DBMS

# Build with default settings
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# The database is now ready to use!
./latticedb
```

## Detailed Platform Instructions

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libreadline-dev \
    liblz4-dev \
    libzstd-dev

# Clone and build
git clone https://github.com/hoangsonww/LatticeDB-DBMS.git
cd LatticeDB-DBMS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
make test

# Optional: Install system-wide
sudo make install
```

### macOS

```bash
# Install Xcode Command Line Tools (if not already installed)
xcode-select --install

# Install Homebrew dependencies
brew install cmake lz4 zstd readline

# Clone and build
git clone https://github.com/hoangsonww/LatticeDB-NextGen-DBMS.git
cd LatticeDB-NextGen-DBMS
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)

# Run tests
make test
```

### Windows

#### Option 1: WSL2 (Recommended)

```bash
# In WSL2 Ubuntu terminal
# Follow Linux instructions above
```

#### Option 2: Visual Studio 2019/2022

1. Install Visual Studio with "Desktop development with C++" workload
2. Install CMake (included with Visual Studio or standalone)
3. Open PowerShell or Command Prompt:

```powershell
# Clone repository
git clone https://github.com/hoangsonww/LatticeDB-DBMS.git
cd LatticeDB-DBMS

# Generate Visual Studio solution
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

# Build using MSBuild
cmake --build . --config Release --parallel

# Or open LatticeDB.sln in Visual Studio and build from IDE
```

#### Option 3: MinGW-w64

```powershell
# Install MSYS2 and MinGW-w64
# In MSYS2 terminal:
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make

# Build
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j4
```

### Docker Build

```bash
# Build Docker image
docker build -t latticedb .

# Run container
docker run -it --rm -v $(pwd)/data:/data latticedb

# Or use docker-compose
docker-compose up -d
```

## CMake Configuration Options

### Build Types

```bash
# Debug build (with debug symbols, no optimization)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release build (optimized, no debug symbols)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Release with debug info
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Minimum size release
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
```

### Feature Flags

```bash
# Enable/disable specific features
cmake .. \
  -DENABLE_COMPRESSION=ON \      # Enable compression support (default: ON)
  -DENABLE_VECTOR_SEARCH=ON \    # Enable vector search engine (default: ON)
  -DENABLE_NETWORK_SERVER=ON \   # Enable network server (default: ON)
  -DBUILD_TESTS=ON \            # Build test suite (default: ON)
  -DBUILD_BENCHMARKS=ON \       # Build benchmarks (default: ON)
  -DBUILD_GUI=ON \              # Build GUI components (default: ON)
  -DENABLE_SANITIZERS=OFF \     # Enable AddressSanitizer (default: OFF)
  -DENABLE_COVERAGE=OFF         # Enable code coverage (default: OFF)
```

### Advanced Options

```bash
# Custom installation prefix
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/latticedb

# Use specific compiler
cmake .. -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++

# Use Ninja instead of Make
cmake .. -G Ninja

# Enable all warnings
cmake .. -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"

# Link-time optimization
cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

# Static linking
cmake .. -DBUILD_SHARED_LIBS=OFF
```

## Build Targets

After configuration, you can build specific targets:

```bash
# Build everything
make all

# Build specific executables
make latticedb          # CLI interface
make latticedb_server   # Network server
make latticedb_test     # Test suite
make latticedb_bench    # Benchmarks

# Run tests
make test
# or
ctest --verbose

# Generate documentation
make docs

# Clean build
make clean

# Install
sudo make install

# Create package (DEB/RPM/TGZ)
make package
```

## Troubleshooting

### Common Issues

#### 1. CMake version too old
```bash
# Ubuntu: Add CMake PPA
sudo apt-add-repository ppa:kitware/cmake
sudo apt-get update
sudo apt-get install cmake

# macOS: Update with Homebrew
brew upgrade cmake
```

#### 2. C++17 compiler not found
```bash
# Ubuntu: Install newer GCC
sudo apt-get install gcc-8 g++-8
export CC=gcc-8
export CXX=g++-8
```

#### 3. Missing compression libraries
```bash
# The build will work without them, but to enable:
# Ubuntu
sudo apt-get install liblz4-dev libzstd-dev

# macOS
brew install lz4 zstd
```

#### 4. Permission denied during install
```bash
# Either use sudo
sudo make install

# Or install to user directory
cmake .. -DCMAKE_INSTALL_PREFIX=$HOME/.local
make install
```

### Debug Build Issues

Enable verbose output:
```bash
make VERBOSE=1
# or
cmake --build . --verbose
```

Clean rebuild:
```bash
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
```

## Verification

After building, verify the installation:

```bash
# Check version
./latticedb --version

# Run simple test
echo "SELECT 1+1;" | ./latticedb

# Run test suite
make test

# Run benchmarks
./latticedb_bench

# Check server
./latticedb_server --help
```

## GUI Setup

The GUI requires Node.js for development:

```bash
# Install Node.js dependencies
cd gui
npm install

# Development mode
npm run dev

# Production build
npm run build

# The GUI will be available at http://localhost:5173
```

## Development Setup

For development, additional tools are recommended:

```bash
# Install development tools
# Ubuntu
sudo apt-get install \
    clang-format \
    clang-tidy \
    cppcheck \
    valgrind \
    gdb

# macOS
brew install \
    clang-format \
    cppcheck \
    valgrind

# Format code
make format

# Run static analysis
make analyze

# Run with sanitizers
cmake .. -DENABLE_SANITIZERS=ON
make
```

## Performance Optimization

For maximum performance:

```bash
# Aggressive optimization
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native" \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON

# Profile-guided optimization (PGO)
# Step 1: Build with profiling
cmake .. -DCMAKE_CXX_FLAGS="-fprofile-generate"
make

# Step 2: Run representative workload
./latticedb_bench

# Step 3: Rebuild with profile data
cmake .. -DCMAKE_CXX_FLAGS="-fprofile-use"
make
```

## Continuous Integration

For CI/CD pipelines:

```yaml
# GitHub Actions example
- name: Build LatticeDB
  run: |
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
    make -j2
    make test
```

## Support

If you encounter issues:

1. Check the [GitHub Issues](https://github.com/hoangsonww/LatticeDB-DBMS/issues)
2. Review build logs carefully
3. Ensure all prerequisites are met
4. Try a clean build in a new directory
5. Open an issue with:
   - Platform and version
   - Compiler version
   - CMake version
   - Complete error output

## Next Steps

After successful build:
- Read the [README](README.md) for usage instructions
- Check [ARCHITECTURE](ARCHITECTURE.md) for system design
- Try the example queries in `examples/`
- Run the test suite to verify functionality
- Start the GUI for visual interaction
