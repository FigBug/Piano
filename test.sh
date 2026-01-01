#!/bin/bash

cd "$(dirname "$0")"

BUILD_TYPE="${1:-Release}"
ENABLE_ASAN="${2:-OFF}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_error() {
    echo -e "${RED}ERROR: $1${NC}" >&2
}

print_success() {
    echo -e "${GREEN}$1${NC}"
}

print_warning() {
    echo -e "${YELLOW}$1${NC}"
}

echo "=========================================="
echo "Building Piano Tests (${BUILD_TYPE}, ASAN=${ENABLE_ASAN})"
echo "=========================================="

# Build step
if ! cmake -B build -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DBUILD_TESTS=ON -DENABLE_ASAN="${ENABLE_ASAN}" 2>&1; then
    print_error "CMake configuration failed!"
    exit 1
fi

if ! cmake --build build --target PianoTests --config "${BUILD_TYPE}" -j 2>&1; then
    print_error "Build failed!"
    exit 1
fi

echo ""
echo "=========================================="
echo "Running Tests"
echo "=========================================="

# Determine executable path based on platform
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" || "$OSTYPE" == "cygwin" ]]; then
    TEST_EXECUTABLE="./build/tests/PianoTests_artefacts/${BUILD_TYPE}/PianoTests.exe"
else
    TEST_EXECUTABLE="./build/tests/PianoTests_artefacts/${BUILD_TYPE}/PianoTests"
fi

# Check if executable exists
if [ ! -f "$TEST_EXECUTABLE" ]; then
    print_error "Test executable not found at: $TEST_EXECUTABLE"
    echo "Available files in build directory:"
    find ./build -name "PianoTests*" -type f 2>/dev/null || echo "  (none found)"
    exit 1
fi

# Run tests and capture exit code
set +e  # Don't exit on error, we want to handle it ourselves
"$TEST_EXECUTABLE" tests/reference
EXIT_CODE=$?
set -e

echo ""
echo "=========================================="

# Interpret exit code
if [ $EXIT_CODE -eq 0 ]; then
    print_success "TEST RUN COMPLETED SUCCESSFULLY"
    exit 0
elif [ $EXIT_CODE -eq 1 ]; then
    print_error "TEST FAILED - One or more tests did not pass"
    exit 1
elif [ $EXIT_CODE -eq 139 ] || [ $EXIT_CODE -eq $((128 + 11)) ]; then
    print_error "TEST CRASHED - Segmentation fault (SIGSEGV)"
    echo "The test executable crashed due to a memory access violation."
    echo "Consider running with ASAN enabled: ./test.sh ${BUILD_TYPE} ON"
    exit 139
elif [ $EXIT_CODE -eq 134 ] || [ $EXIT_CODE -eq $((128 + 6)) ]; then
    print_error "TEST CRASHED - Abort signal (SIGABRT)"
    echo "The test executable was aborted, possibly due to an assertion failure."
    exit 134
elif [ $EXIT_CODE -eq 136 ] || [ $EXIT_CODE -eq $((128 + 8)) ]; then
    print_error "TEST CRASHED - Floating point exception (SIGFPE)"
    echo "The test executable crashed due to a floating point error (e.g., division by zero)."
    exit 136
elif [ $EXIT_CODE -eq 137 ] || [ $EXIT_CODE -eq $((128 + 9)) ]; then
    print_error "TEST KILLED - Process was killed (SIGKILL)"
    echo "The test was killed, possibly due to timeout or out of memory."
    exit 137
elif [ $EXIT_CODE -eq 132 ] || [ $EXIT_CODE -eq $((128 + 4)) ]; then
    print_error "TEST CRASHED - Illegal instruction (SIGILL)"
    echo "The test executable encountered an illegal CPU instruction."
    exit 132
elif [ $EXIT_CODE -gt 128 ]; then
    SIGNAL=$((EXIT_CODE - 128))
    print_error "TEST CRASHED - Terminated by signal ${SIGNAL}"
    echo "The test executable was terminated by an unexpected signal."
    exit $EXIT_CODE
else
    print_error "TEST FAILED - Exit code: ${EXIT_CODE}"
    exit $EXIT_CODE
fi
