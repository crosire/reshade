# HDR Hide Addon

This example addon demonstrates how to use the `reshade_overlay_check_color_space_support` event to hide HDR color space support when auto-enabled.

## Features

- **HDR Hiding**: Automatically hides HDR10 ST2084 and HDR10 HLG color spaces from overlay color space support checks
- **Toggle Control**: Provides a checkbox in the overlay to enable/disable HDR hiding
- **Visual Feedback**: Shows the current state (HDR hidden/visible) with colored text

## How It Works

The addon registers for the `reshade_overlay_check_color_space_support` event, which is called whenever ReShade checks if a color space is supported for overlay rendering. When this event is triggered:

1. The addon checks if HDR hiding is enabled
2. If enabled and the color space is HDR10 ST2084 or HDR10 HLG, it sets `supported` to `false`
3. Returns `true` to indicate it has overridden the result
4. For all other color spaces or when hiding is disabled, it returns `false` to let ReShade handle the check normally

## Usage

1. Build the addon using Visual Studio or MSBuild
2. Copy the generated `.addon32` or `.addon64` file to your ReShade addons directory
3. Load the addon in ReShade
4. Use the checkbox in the overlay to toggle HDR hiding on/off

## Technical Details

- **Event**: `reshade_overlay_check_color_space_support`
- **Target APIs**: D3D11 and D3D12 (only APIs that support HDR through DXGI)
- **Color Spaces Affected**: `hdr10_st2084`, `hdr10_hlg`
- **Purpose**: Prevents HDR from being automatically enabled for overlays

## Building

This addon can be built as part of the Examples solution or individually using MSBuild:

```cmd
msbuild examples\18-hdr_hide\hdr_hide.vcxproj /p:Configuration=Release /p:Platform=x64
```

The output will be `hdr_hide.addon64` in the `bin\x64\Release Examples\` directory.
