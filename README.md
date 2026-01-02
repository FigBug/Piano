# Piano

[![Build macOS](https://github.com/FigBug/Piano/actions/workflows/build_macos.yaml/badge.svg)](https://github.com/FigBug/Piano/actions/workflows/build_macos.yaml)
[![Build Windows](https://github.com/FigBug/Piano/actions/workflows/build_windows.yaml/badge.svg)](https://github.com/FigBug/Piano/actions/workflows/build_windows.yaml)
[![Build Linux](https://github.com/FigBug/Piano/actions/workflows/build_linux.yaml/badge.svg)](https://github.com/FigBug/Piano/actions/workflows/build_linux.yaml)
[![Tests](https://github.com/FigBug/Piano/actions/workflows/test.yaml/badge.svg)](https://github.com/FigBug/Piano/actions/workflows/test.yaml)
[![Performance](https://github.com/FigBug/Piano/actions/workflows/performance.yaml/badge.svg)](https://figbug.github.io/Piano/dev/bench/)

A physically modeled piano synthesizer plugin using digital waveguide synthesis.

## Features

- Physical modeling synthesis based on digital waveguide string models
- Hammer-string interaction simulation
- Longitudinal string modes
- Soundboard resonance
- Available as VST, VST3, AU (macOS), and LV2 (Linux) plugins

## Building

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler
- Platform-specific dependencies:
  - **macOS**: Xcode
  - **Windows**: Visual Studio 2019 or higher
  - **Linux**: GCC/Clang, ALSA, JACK, and X11 development libraries

### Build Steps

```bash
# Configure
cmake -B build

# Build
cmake --build build --config Release
```

The built plugins will be in `build/Piano_artefacts/Release/`.

## Testing

The project includes a comprehensive test suite with unit tests for internal components and integration tests for audio output.

### Running Tests

```bash
# Build and run tests
./test.sh

# Run with AddressSanitizer
./test.sh Release ON

# Run in Debug mode
./test.sh Debug
```

### Test Suite

- **Unit Tests**: Filter, Hammer, MSD2Filter, ResampleFIR, DWGS (Digital Waveguide String) classes
- **Integration Tests**: Scale test, Chord test, 4-minute stress test

## License

This project is licensed under the GPL-3.0 License.

## Credits

Based on physical modeling research and digital waveguide synthesis techniques.
