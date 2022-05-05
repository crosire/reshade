#pragma once

// This header contains macros for keyboard keys and mouse buttons.
// These are not in the "VK_[key]" format, but instead adapted for more intuitive naming.
// Source for the key codes: https://msdn.microsoft.com/pt-br/library/windows/desktop/dd375731(v=vs.85).aspx

//Mouse Buttons//////////////////////////////////////////////////////////////////////////////////////////////////

#define MOUSE_LEFT 0
#define MOUSE_RIGHT 1
#define MOUSE_MIDDLE 2

//Numerical Keys/////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_0 0x30
#define KEY_1 0x31
#define KEY_2 0x32
#define KEY_3 0x33
#define KEY_4 0x34
#define KEY_5 0x35
#define KEY_6 0x36
#define KEY_7 0x37
#define KEY_8 0x38
#define KEY_9 0x39

//Numpad Keys////////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_NUMPAD0 0x60
#define KEY_NUMPAD1 0x61
#define KEY_NUMPAD2 0x62
#define KEY_NUMPAD3 0x63
#define KEY_NUMPAD4 0x64
#define KEY_NUMPAD5 0x65
#define KEY_NUMPAD6 0x66
#define KEY_NUMPAD7 0x67
#define KEY_NUMPAD8 0x68
#define KEY_NUMPAD9 0x69

//Alphabetical Keys//////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_A 0x41
#define KEY_B 0x42
#define KEY_C 0x43
#define KEY_D 0x44
#define KEY_E 0x45
#define KEY_F 0x46
#define KEY_G 0x47
#define KEY_H 0x48
#define KEY_I 0x49
#define KEY_J 0x4A
#define KEY_K 0x4B
#define KEY_L 0x4C
#define KEY_M 0x4D
#define KEY_N 0x4E
#define KEY_O 0x4F
#define KEY_P 0x50
#define KEY_Q 0x51
#define KEY_R 0x52
#define KEY_S 0x53
#define KEY_T 0x54
#define KEY_U 0x55
#define KEY_V 0x56
#define KEY_W 0x57
#define KEY_X 0x58
#define KEY_Y 0x59
#define KEY_Z 0x5A

//Common Keys////////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_TAB 0x09
#define KEY_RETURN 0x0D
#define KEY_ENTER KEY_RETURN
#define KEY_SHIFT 0x10
#define KEY_CONTROL 0x11
#define KEY_ALT 0x12
#define KEY_PAUSE 0x13
#define KEY_CAPSLOCK 0x14
#define KEY_SPACE 0x20
#define KEY_LEFT 0x25
#define KEY_UP 0x26
#define KEY_RIGHT 0x27
#define KEY_DOWN 0x28
#define KEY_BACKSPACE 0x08
#define KEY_ESCAPE 0x1B
#define KEY_ESC KEY_ESCAPE
#define KEY_END 0x23
#define KEY_HOME 0x24
#define KEY_SELECT 0x29
#define KEY_EXECUTE 0x2B
#define KEY_PRINTSCREEN 0x2C
#define KEY_INSERT 0x2D
#define KEY_DELETE 0x2E
#define KEY_LWIN 0x5B
#define KEY_RWIN 0x5C
#define KEY_MULTIPLY 0x6A
#define KEY_ADD 0x6B
#define KEY_SEPARATOR 0x6C
#define KEY_SUBTRACT 0x6D
#define KEY_DECIMAL 0x6E
#define KEY_DIVIDE 0x6F
#define KEY_NUMLOCK 0x90
#define KEY_SCROLL 0x91
#define KEY_LSHIFT 0xA0
#define KEY_RSHIFT 0xA1
#define KEY_LCONTROL 0xA2
#define KEY_RCONTROL 0xA3
#define KEY_LALT 0xA4
#define KEY_RALT 0xA5
#define KEY_SEMICOLON 0xBA
#define KEY_PLUS 0xBB
#define KEY_COMMA 0xBC
#define KEY_MINUS 0xBD
#define KEY_PERIOD 0xBE
#define KEY_SLASH 0xBF
#define KEY_TILDE 0xC0
#define KEY_ANGLE_BRACKET 0xE2
#define KEY_LBRACKET 0xDB
#define KEY_BACKSLASH 0xDC
#define KEY_RBRACKET 0xDD
#define KEY_QUOTES 0xDE

//Function Keys//////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_F1 0x70
#define KEY_F2 0x71
#define KEY_F3 0x72
#define KEY_F4 0x73
#define KEY_F5 0x74
#define KEY_F6 0x75
#define KEY_F7 0x76
#define KEY_F8 0x77
#define KEY_F9 0x78
#define KEY_F10 0x79
#define KEY_F11 0x7A
#define KEY_F12 0x7B
#define KEY_F13 0x7C
#define KEY_F14 0x7D
#define KEY_F15 0x7E
#define KEY_F16 0x7F
#define KEY_F17 0x80
#define KEY_F18 0x81
#define KEY_F19 0x82
#define KEY_F20 0x83
#define KEY_F21 0x84
#define KEY_F22 0x85
#define KEY_F23 0x86
#define KEY_F24 0x87

//Media Keys/////////////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_VOLUME_MUTE 0xAD
#define KEY_VOLUME_DOWN 0xAE
#define KEY_VOLUME_UP 0xAF
#define KEY_MEDIA_NEXT_TRACK 0xB0
#define KEY_MEDIA_PREV_TRACK 0xB1
#define KEY_MEDIA_STOP 0xB2
#define KEY_MEDIA_PLAY_PAUSE 0xB3

//Miscellaneous Keys/////////////////////////////////////////////////////////////////////////////////////////////

#define KEY_NONE 0x00
#define KEY_LBUTTON 0x01
#define KEY_RBUTTON 0x02
#define KEY_CANCEL 0x03
#define KEY_MBUTTON 0x04
#define KEY_XBUTTON1 0x05
#define KEY_XBUTTON2 0x06
#define KEY_CLEAR 0x0C
#define KEY_KANA 0x15
#define KEY_HANGUEL 0x15
#define KEY_HANGUL 0x15
#define KEY_JUNJA 0x17
#define KEY_FINAL 0x18
#define KEY_HANJA 0x19
#define KEY_KANJI 0x19
#define KEY_CONVERT 0x1C
#define KEY_NONCONVERT 0x1D
#define KEY_ACCEPT 0x1E
#define KEY_MODECHANGE 0x1F
#define KEY_PRIOR 0x21
#define KEY_NEXT 0x22
#define KEY_HELP 0x2F
#define KEY_APPS 0x5D
#define KEY_SLEEP 0x5F
#define KEY_BROWSER_BACK 0xA6
#define KEY_BROWSER_FORWARD 0xA7
#define KEY_BROWSER_REFRESH 0xA8
#define KEY_BROWSER_STOP 0xA9
#define KEY_BROWSER_SEARCH 0xAA
#define KEY_BROWSER_FAVORITES 0xAB
#define KEY_BROWSER_HOME 0xAC
#define KEY_LAUNCH_MAIL 0xB4
#define KEY_LAUNCH_MEDIA_SELECT 0xB5
#define KEY_LAUNCH_APP1 0xB6
#define KEY_LAUNCH_APP2 0xB7
#define KEY_OEM_8 0xDF
#define KEY_PROCESSKEY 0xE5
#define KEY_PACKET 0xE7
#define KEY_ATTN 0xF6
#define KEY_CRSEL 0xF7
#define KEY_EXSEL 0xF8
#define KEY_EREOF 0xF9
#define KEY_PLAY 0xFA
#define KEY_ZOOM 0xFB
#define KEY_NONAME 0xFC
#define KEY_PA1 0xFD
#define KEY_OEM_CLEAR 0xFE
#define KEY_PRINT 0x2A
