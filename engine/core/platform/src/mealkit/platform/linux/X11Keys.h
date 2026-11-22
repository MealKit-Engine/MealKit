#pragma once

// X11 key symbols → Win32 virtual key codes (the codes in platform/Input.h). Shared by Window.cpp and ImGuiPlatform.cpp.
// Keys are read from the layout's first level (XkbKeycodeToKeysym level 0), so letters map to the uppercase code like Windows.
#include "mealkit/core/Types.h"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

namespace mealkit::x11 {

inline u8 vkFromKeysym(KeySym sym) {
    if (sym >= XK_a && sym <= XK_z) return static_cast<u8>('A' + (sym - XK_a));
    if (sym >= XK_A && sym <= XK_Z) return static_cast<u8>('A' + (sym - XK_A));
    if (sym >= XK_0 && sym <= XK_9) return static_cast<u8>('0' + (sym - XK_0));
    if (sym >= XK_F1 && sym <= XK_F24) return static_cast<u8>(0x70 + (sym - XK_F1));
    if (sym >= XK_KP_0 && sym <= XK_KP_9) return static_cast<u8>(0x60 + (sym - XK_KP_0));
    switch (sym) {
    case XK_BackSpace: return 0x08;
    case XK_Tab:
    case XK_ISO_Left_Tab: return 0x09;
    case XK_Return:
    case XK_KP_Enter: return 0x0D;
    case XK_Shift_L:
    case XK_Shift_R: return 0x10;
    case XK_Control_L:
    case XK_Control_R: return 0x11;
    case XK_Alt_L:
    case XK_Alt_R:
    case XK_Meta_L:
    case XK_Meta_R: return 0x12;
    case XK_Pause: return 0x13;
    case XK_Caps_Lock: return 0x14;
    case XK_Hangul: return 0x15;       // VK_HANGUL
    case XK_Hangul_Hanja: return 0x19; // VK_HANJA
    case XK_Escape: return 0x1B;
    case XK_space: return 0x20;
    case XK_Prior:
    case XK_KP_Prior: return 0x21;
    case XK_Next:
    case XK_KP_Next: return 0x22;
    case XK_End:
    case XK_KP_End: return 0x23;
    case XK_Home:
    case XK_KP_Home: return 0x24;
    case XK_Left:
    case XK_KP_Left: return 0x25;
    case XK_Up:
    case XK_KP_Up: return 0x26;
    case XK_Right:
    case XK_KP_Right: return 0x27;
    case XK_Down:
    case XK_KP_Down: return 0x28;
    case XK_Print: return 0x2C;
    case XK_Insert:
    case XK_KP_Insert: return 0x2D;
    case XK_Delete:
    case XK_KP_Delete: return 0x2E;
    case XK_Super_L: return 0x5B;
    case XK_Super_R: return 0x5C;
    case XK_Menu: return 0x5D;
    case XK_KP_Multiply: return 0x6A;
    case XK_KP_Add: return 0x6B;
    case XK_KP_Subtract: return 0x6D;
    case XK_KP_Decimal: return 0x6E;
    case XK_KP_Divide: return 0x6F;
    case XK_Num_Lock: return 0x90;
    case XK_Scroll_Lock: return 0x91;
    case XK_semicolon: return 0xBA;
    case XK_equal: return 0xBB;
    case XK_comma: return 0xBC;
    case XK_minus: return 0xBD;
    case XK_period: return 0xBE;
    case XK_slash: return 0xBF;
    case XK_grave: return 0xC0;
    case XK_bracketleft: return 0xDB;
    case XK_backslash: return 0xDC;
    case XK_bracketright: return 0xDD;
    case XK_apostrophe: return 0xDE;
    default: return 0;
    }
}

// Virtual key of a key event, from the unshifted symbol of its key
inline u8 vkFromEvent(Display* display, const XKeyEvent& event) {
    return vkFromKeysym(XkbKeycodeToKeysym(display, static_cast<KeyCode>(event.keycode), 0, 0));
}

} // namespace mealkit::x11
