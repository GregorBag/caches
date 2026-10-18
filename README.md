# Caches

C++23 cache library implementing ARC, IDEAL, LFU, LIRS, LRU, and 2Q.

## Requirements

- Linux
- CMake 3.28+
- GCC 14+ (C and C++)
- Ninja 1.11+
- Git and internet access to download dependencies

GoogleTest is fetched automatically via CPM.cmake.

## Building

Configure and build from the repository root:

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=Release
```
Tests are enabled by default. Add `-DBUILD_TESTS=OFF` to configure a library-only build.

```bash
cmake --build build -j2
```

## Running

The project builds a library. Run the test executable directly:

```bash
./build/tests/cache_tests
```

## Testing

Run all tests through CTest, showing output on failure:

```bash
ctest --test-dir build --output-on-failure
```
