# Input Control Addon

This addon demonstrates the new input blocking API functions in ReShade.

## Overview

This feature exposes Reshade's keyboard/mouse input blocking API's already used while UI overlay is open.

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

## Testing

1. Load the addon in a game with ReShade
2. Open the ReShade overlay (usually Home key)
3. Navigate to the addon settings
4. Enable input blocking through Addon''s UI.
5. Disable overlay (using Home)
6. Game shouldn''t accept any input anymore.