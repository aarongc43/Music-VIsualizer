# AGENTS.md

This file contains guidelines and commands for agentic coding agents working in the Bragi Beats music visualizer codebase.

## Project Overview

Bragi Beats is a C-based audio visualizer that uses FFT analysis to create real-time visualizations from audio files. The project uses:
- **Language**: C11 standard
- **Graphics**: RayLib library for rendering and audio
- **Testing**: Unity testing framework
- **Build System**: GNU Make
- **Platform**: macOS (with Homebrew dependencies)

## Build Commands

### Primary Build Commands
```bash
# Build the main application
make all                    # Builds bragibeats executable

# Clean build artifacts
make clean                  # Removes all .o files and executables

# Build FFT benchmark tool
make fft_bench              # Builds performance testing tool
```

### Testing Commands
```bash
# Run all tests
make test                   # Runs all test suites

# Run individual test modules
make run_tests_audio_engine         # Test audio loading/processing
make run_tests_event_system         # Test event system
make run_tests_fft                  # Test FFT implementation
make run_tests_visualization_engine # Test visualization components
make run_tests_fft_verification     # Test FFT accuracy
```

### Running the Application
```bash
# After building with make all
./bragibeats                # Run the main visualizer application
```

## Code Structure

### Directory Layout
```
src/
├── core/           # Core application logic (main loop, event system)
├── audio/          # Audio processing, WAV loading, FFT
├── visualization/  # Rendering and visual effects
└── tools/          # Utility tools (benchmarks, etc.)
tests/              # Unit tests using Unity framework
music/              # Audio files for testing
```

### Key Components
- `src/core/app_core.c` - Main application entry point and game loop
- `src/core/event_system.c` - Event-driven architecture for decoupled communication
- `src/audio/audio_engine.c` - WAV file loading and audio processing
- `src/audio/fft/` - Fast Fourier Transform implementation
- `src/visualization/` - Real-time visualization rendering

## Code Style Guidelines

### Naming Conventions
- **Functions**: `snake_case` (e.g., `wav_load`, `es_register`, `vis_init`)
- **Variables**: `snake_case` for local variables, `g_` prefix for globals
- **Types**: `PascalCase` for typedefs (e.g., `WAV`, `EventHandler`, `EventType`)
- **Constants**: `UPPER_SNAKE_CASE` with descriptive names
- **File names**: `snake_case.c` and `snake_case.h`

### Headers and Includes
- Include order: System headers → Local headers (relative path)
- Use `#ifndef GUARD_NAME_H` / `#define GUARD_NAME_H` for header guards
- Guard names should match filename in `UPPER_SNAKE_CASE`
- Keep includes minimal and specific to what's needed

```c
// Example include order
#include <stdio.h>
#include <stdlib.h>
#include "audio_engine.h"
#include "event_system.h"
```

### Function Documentation
- Use concise comments above function declarations in headers
- Focus on purpose, parameters, and return values
- Keep comments brief but informative

```c
/**
 * load a 16-bit PCM WAV file
 * @param filepath path to .wav
 * @param out_wav pointer to WAV struct to fill
 * @return 0 on success, non-zero on error
 */
int wav_load(const char *filepath, WAV *out_wav);
```

### Error Handling
- Return integers (0 for success, non-zero for error) from functions that can fail
- Use `fprintf(stderr, "...")` for error reporting with consistent format
- Always clean up resources (free memory, close files) on error paths
- Validate pointers and parameters early in functions

### Memory Management
- Always check malloc return values
- Pair every malloc with corresponding free
- Use descriptive variable names for buffers
- Initialize pointers to NULL where appropriate

### Code Formatting
- **Indentation**: 4 spaces (no tabs)
- **Braces**: K&R style (opening brace on same line)
- **Spacing**: Around operators, after commas, in function calls
- **Line length**: Prefer under 100 characters
- **Comments**: Use `//` for brief notes, `/* */` for longer explanations

### Testing Patterns
- Use Unity framework assertions: `TEST_ASSERT_EQUAL_INT`, `TEST_ASSERT_NOT_NULL`, etc.
- Implement `setUp()` and `tearDown()` functions for test fixtures
- Name tests descriptively: `test_function_scenario()`
- Test both success and failure paths
- Use fixture files in `tests/fixtures/` for test data

### Event System Usage
- Register event handlers using `es_register(EventType, EventHandler)`
- Emit events with `es_emit(EventType, void* payload)`
- Always include `EVT_COUNT` as the final enum value
- Keep event payloads simple and well-defined

### FFT Integration
- Use power-of-2 buffer sizes (commonly 1 << 13 = 8192)
- Initialize FFT subsystem with `fft_init(size)` before use
- Compute magnitudes with `fft_compute(time_domain, freq_domain)`
- Register `EVT_FFT_READY` handler to receive results

## Development Workflow

1. **Before making changes**: Run `make test` to ensure baseline functionality
2. **Making changes**: Follow existing code patterns and naming conventions
3. **Building**: Use `make all` to build the main application
4. **Testing**: Run relevant individual tests or full test suite
5. **Debugging**: Use `fft_bench` tool for performance testing

## Dependencies

- **raylib**: Install via Homebrew: `brew install raylib`
- **clang**: Default macOS compiler, configured via Makefile
- **Unity framework**: Included in `tests/vendor/unity/`

## Common Patterns

### Module Structure
```c
// module_name.h
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

// Type definitions
typedef struct { ... } ModuleType;

// Public functions
int module_init(void);
void module_shutdown(void);
int module_do_work(ModuleType *module);

#endif
```

### Error Handling Pattern
```c
int function_that_can_fail(const char *input) {
    if (!input) {
        fprintf(stderr, "[module] ERROR: null input\n");
        return -1;
    }
    
    // Do work
    if (error_occurred) {
        fprintf(stderr, "[module] ERROR: specific error\n");
        cleanup();
        return -2;
    }
    
    return 0; // Success
}
```

### Global Variable Pattern
```c
static Type g_global_variable;  // Use 'static' for file scope

// Initialize in module_init()
// Access through getter functions when possible
```