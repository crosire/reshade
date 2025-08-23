# Input Control Addon

This addon demonstrates the new input blocking API functions in ReShade.

## Overview

The input blocking API allows addons to control input blocking independently of the overlay state. This can be useful for:

- Creating custom input handling systems
- Implementing accessibility features
- Building specialized input processing addons
- Testing input blocking behavior

## API Functions

### `block_mouse_input(runtime, enable)`
- **Parameters:**
  - `runtime`: The effect runtime to control input blocking for
  - `enable`: Whether to enable mouse input blocking
- **Description:** Blocks or unblocks mouse input from reaching the application

### `block_keyboard_input(runtime, enable)`
- **Parameters:**
  - `runtime`: The effect runtime to control input blocking for
  - `enable`: Whether to enable keyboard input blocking
- **Description:** Blocks or unblocks keyboard input from reaching the application

### `block_mouse_cursor_warping(runtime, enable)`
- **Parameters:**
  - `runtime`: The effect runtime to control input blocking for
  - `enable`: Whether to enable mouse cursor warping blocking
- **Description:** Blocks or unblocks mouse cursor warping (SetCursorPos calls)

## Usage Example

```cpp
#include <reshade.hpp>

static void on_reshade_present(reshade::api::effect_runtime *runtime)
{
    // Block mouse input
    reshade::block_mouse_input(runtime, true);
    
    // Block keyboard input
    reshade::block_keyboard_input(runtime, true);
    
    // Block mouse cursor warping
    reshade::block_mouse_cursor_warping(runtime, true);
}

## Important Notes

⚠️ **STABLE API**: This API is now stable and ready for production use.

⚠️ **Use with Caution**: These functions can completely block user input to the application. Make sure to provide a way for users to re-enable input or exit the addon.

⚠️ **Runtime-Specific**: Input blocking is controlled per effect runtime. If you have multiple runtimes (e.g., desktop + VR), you need to control each one separately.

## Building

1. Open the `input_control.vcxproj` file in Visual Studio
2. Build the project for your target platform (x86 or x64)
3. Copy the resulting DLL to your ReShade addons folder

## Testing

1. Load the addon in a game with ReShade
2. Open the ReShade overlay (usually Home key)
3. Navigate to the addon settings
4. Use the checkboxes to manually control input blocking
5. The addon also automatically toggles input blocking every 5 seconds for demonstration

## Dependencies

- ReShade 5.0 or later
- Dear ImGui (included with ReShade)
- Windows SDK 10.0 or later
