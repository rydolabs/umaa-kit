# UMAA Kit SDK — tested prototype installation paths

> These are **prototype** installation instructions; the installed headers
> do not contain complete UMAA 6.0 DDS services. Official AUVSI IDL and vendor
> DDS packages are not bundled. Review their licenses/terms independently.

## Requirements

- CMake 3.20+ and a C++17 compiler (GCC/Clang on Linux, MSVC on Windows).
- CycloneDDS C development package, required only for `UMAA::Cyclone`.
- Python 3.10+ if you run the code generator or test suite.

## Install core without CycloneDDS

From the root of this repository:

```sh
cmake -S sdk -B build/sdk-release -DUMAA_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/sdk-install"
cmake --build build/sdk-release --config Release
cmake --install build/sdk-release --config Release
```

An independent application's `CMakeLists.txt` can then use:

```cmake
find_package(UMAAPrototype CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE UMAA::Core)
```

Set `CMAKE_PREFIX_PATH` to the SDK install prefix in the consumer project.
The package is currently named `UMAAPrototype` (version `0.1.0`), and its
public interface is not version-stable.

## Optional CycloneDDS backend

With a compatible installed CycloneDDS C package discoverable by CMake:

```sh
cmake -S sdk -B build/sdk-cyclone \
  -DUMAA_BUILD_TESTS=OFF -DUMAA_ENABLE_CYCLONE_ADAPTER=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/sdk-install"
cmake --build build/sdk-cyclone --config Release
cmake --install build/sdk-cyclone --config Release
```

External consumers can link `UMAA::Cyclone` after
`find_package(UMAAPrototype CONFIG REQUIRED)`. CMake resolves the vendor
`CycloneDDS::ddsc` dependency; the vendor must already be installed.
The SDK does **not** infer required UMAA Topic names, TypeSupport, keys, QoS,
or service roles. Callers must use reviewed official service contracts.

## Verify the source checkout

```sh
python tools/ci_verify.py --compiler default
```

Real official UMAA 6.0 CycloneDDS process tests additionally require the
pinned AUVSI input and are executed on GitHub Actions Linux and Windows.
See [G1–G12 delivery gates](DELIVERY_GATES.md) for limits and acceptance.
