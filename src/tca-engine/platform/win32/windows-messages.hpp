// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#pragma once

// C++ STL
#include <cstdint>

#include "platform/win32/windows.hpp"

namespace tca::engine::platform::win32 {

/**
 * @brief Simple enum class representing the Windows messages that can be received by a window
 * procedure. This is used to improve debugging and readability of the code for the most common
 * messages, but it can be easily extended to include more messages if needed. The values of the
 * enum correspond to the Windows message constants defined in the Windows API.
 */
enum class windows_messages : std::uint32_t {
    null = WM_NULL,
    create = WM_CREATE,
    destroy = WM_DESTROY,
    move = WM_MOVE,
    size = WM_SIZE,
    activate = WM_ACTIVATE,
    set_focus = WM_SETFOCUS,
    kill_focus = WM_KILLFOCUS,
    enable = WM_ENABLE,
    set_redraw = WM_SETREDRAW,
    set_text = WM_SETTEXT,
    get_text = WM_GETTEXT,
    get_text_length = WM_GETTEXTLENGTH,
    paint = WM_PAINT,
    close = WM_CLOSE,
    queryendsession = WM_QUERYENDSESSION,
    quit = WM_QUIT,
    queryopen = WM_QUERYOPEN,
    erase_background = WM_ERASEBKGND,     // 20
    sys_color_change = WM_SYSCOLORCHANGE, // 21
    end_session = WM_ENDSESSION,          // 22
    show_window = WM_SHOWWINDOW,          // 24
    ctl_color = 25U,                      // 25
    setting_change = WM_SETTINGCHANGE,    // 26
    dev_mode_change = WM_DEVMODECHANGE,   // 27
    activate_app = WM_ACTIVATEAPP,        // 28
    font_change = WM_FONTCHANGE,          // 29
    time_change = WM_TIMECHANGE,          // 30
    cancel_mode = WM_CANCELMODE,          // 31
    set_cursor = WM_SETCURSOR,            // 32
    mouse_activate = WM_MOUSEACTIVATE,
    child_activate = WM_CHILDACTIVATE,
    queue_sync = WM_QUEUESYNC,
    get_min_max_info = WM_GETMINMAXINFO,
    paint_icon = WM_PAINTICON,
    icon_erase_background = WM_ICONERASEBKGND,
    next_dlg_ctl = WM_NEXTDLGCTL,
    spooler_status = WM_SPOOLERSTATUS,
    draw_item = WM_DRAWITEM,
    measure_item = WM_MEASUREITEM,
    delete_item = WM_DELETEITEM,
    vkey_to_item = WM_VKEYTOITEM,
    char_to_item = WM_CHARTOITEM,
    set_font = WM_SETFONT,
    get_font = WM_GETFONT,
    set_hotkey = WM_SETHOTKEY,
    get_hotkey = WM_GETHOTKEY,
    query_drag_icon = WM_QUERYDRAGICON,
    compare_item = WM_COMPAREITEM,
    get_object = WM_GETOBJECT,
    compacting = WM_COMPACTING,
    comm_notify = WM_COMMNOTIFY,
    window_position_changing = WM_WINDOWPOSCHANGING, // 70
    window_position_changed = WM_WINDOWPOSCHANGED,   // 71
    power = WM_POWER,                                // 72
    // copy_global_data = WM_COPYGLOBALDATA,         // 73
    copy_data = WM_COPYDATA,                               // 74
    cancel_journal = WM_CANCELJOURNAL,                     // 75
    notify = WM_NOTIFY,                                    // 78
    input_lang_change_request = WM_INPUTLANGCHANGEREQUEST, // 80
    input_lang_change = WM_INPUTLANGCHANGE,
    t_card = WM_TCARD,
    help = WM_HELP,
    user_changed = WM_USERCHANGED,
    notify_format = WM_NOTIFYFORMAT,
    context_menu = WM_CONTEXTMENU,                      // 123
    style_changing = WM_STYLECHANGING,                  // 124
    style_changed = WM_STYLECHANGED,                    // 125
    display_change = WM_DISPLAYCHANGE,                  // 126
    get_icon = WM_GETICON,                              // 127
    set_icon = WM_SETICON,                              // 128
    nc_create = WM_NCCREATE,                            // 129
    nc_destroy = WM_NCDESTROY,                          // 130
    nc_calc_size = WM_NCCALCSIZE,                       // 131
    nc_hit_test = WM_NCHITTEST,                         // 132
    nc_paint = WM_NCPAINT,                              // 133
    nc_activate = WM_NCACTIVATE,                        // 134
    get_dialog_code = WM_GETDLGCODE,                    // 135
    sync_paint = WM_SYNCPAINT,                          // 136
    nc_mouse_move = WM_NCMOUSEMOVE,                     // 160
    nc_lbutton_down = WM_NCLBUTTONDOWN,                 // 161
    nc_lbutton_up = WM_NCLBUTTONUP,                     // 162
    nc_lbutton_double_click = WM_NCLBUTTONDBLCLK,       // 163
    nc_right_button_down = WM_NCRBUTTONDOWN,            // 164
    nc_right_button_up = WM_NCRBUTTONUP,                // 165
    nc_right_button_double_click = WM_NCRBUTTONDBLCLK,  // 166
    nc_middle_button_down = WM_NCMBUTTONDOWN,           // 167
    nc_middle_button_up = WM_NCMBUTTONUP,               // 168
    nc_middle_button_double_click = WM_NCMBUTTONDBLCLK, // 169
    nc_x_button_down = WM_NCXBUTTONDOWN,                // 171
    nc_x_button_up = WM_NCXBUTTONUP,                    // 172
    nc_x_button_double_click = WM_NCXBUTTONDBLCLK,      // 173
    get_sel = EM_GETSEL,                                // 176
    set_sel = EM_SETSEL,                                // 177
    key_down = WM_KEYDOWN,                              // 256
    key_up = WM_KEYUP,                                  // 257
    character = WM_CHAR,                                // 258 ('char' is a C++ keyword)
    dead_char = WM_DEADCHAR,                            // 259
    sys_key_down = WM_SYSKEYDOWN,                       // 260
    sys_key_up = WM_SYSKEYUP,                           // 261
    sys_char = WM_SYSCHAR,                              // 262
    sys_dead_char = WM_SYSDEADCHAR,                     // 263
    uni_char = WM_UNICHAR,                              // 265
    ime_start_composition = WM_IME_STARTCOMPOSITION,    // 269
    ime_end_composition = WM_IME_ENDCOMPOSITION,        // 270
    ime_composition = WM_IME_COMPOSITION,               // 271
    init_dialog = WM_INITDIALOG,                        // 272
    syscommand = WM_SYSCOMMAND,                         // 274
    timer = WM_TIMER,                                   // 275
    hscroll = WM_HSCROLL,                               // 276
    vscroll = WM_VSCROLL,                               // 277
    init_menu = WM_INITMENU,                            // 278
    init_menu_popup = WM_INITMENUPOPUP,                 // 279
    menu_select = WM_MENUSELECT,                        // 287
    menu_char = WM_MENUCHAR,                            // 288
    enter_idle = WM_ENTERIDLE,                          // 289
    uninit_menu_popup = WM_UNINITMENUPOPUP,             // 293
    menu_command = WM_MENUCOMMAND,                      // 294
    change_ui_state = WM_CHANGEUISTATE,                 // 295
    update_ui_state = WM_UPDATEUISTATE,                 // 296
    query_ui_state = WM_QUERYUISTATE,                   // 297
    ctl_color_msg_box = WM_CTLCOLORMSGBOX,              // 306
    ctl_color_edit = WM_CTLCOLOREDIT,                   // 307
    ctl_color_list_box = WM_CTLCOLORLISTBOX,            // 308
    ctl_color_btn = WM_CTLCOLORBTN,                     // 309
    ctl_color_dlg = WM_CTLCOLORDLG,                     // 310
    ctl_color_scroll_bar = WM_CTLCOLORSCROLLBAR,        // 311
    ctl_color_static = WM_CTLCOLORSTATIC,               // 312
    mouse_move = WM_MOUSEMOVE,                          // 512
    left_button_down = WM_LBUTTONDOWN,                  // 513
    left_button_up = WM_LBUTTONUP,                      // 514
    left_button_double_click = WM_LBUTTONDBLCLK,        // 515
    right_button_down = WM_RBUTTONDOWN,                 // 516
    right_button_up = WM_RBUTTONUP,                     // 517
    right_button_double_click = WM_RBUTTONDBLCLK,       // 518
    middle_button_down = WM_MBUTTONDOWN,                // 519
    middle_button_up = WM_MBUTTONUP,                    // 520
    middle_button_double_click = WM_MBUTTONDBLCLK,      // 521
    mouse_wheel = WM_MOUSEWHEEL,                        // 522
    x_button_down = WM_XBUTTONDOWN,                     // 523
    x_button_up = WM_XBUTTONUP,                         // 524
    x_button_double_click = WM_XBUTTONDBLCLK,           // 525
    mouse_horizontal_wheel = WM_MOUSEHWHEEL,            // 526
    sizing = WM_SIZING,                                 // 532
    capture_changed = WM_CAPTURECHANGED,                // 533
    power_broadcast = WM_POWERBROADCAST,                // 536
    device_change = WM_DEVICECHANGE,                    // 537
    moving = WM_MOVING,                                 // 543
    enter_size_move = WM_ENTERSIZEMOVE,                 // 561
    exit_size_move = WM_EXITSIZEMOVE,                   // 562
    drop_files = WM_DROPFILES,                          // 563
    ime_set_context = WM_IME_SETCONTEXT,                // 641
    ime_notify = WM_IME_NOTIFY,                         // 642
    ime_char = WM_IME_CHAR,                             // 646
    nc_mouse_hover = WM_NCMOUSEHOVER,                   // 672
    mouse_hover = WM_MOUSEHOVER,                        // 673
    nc_mouse_leave = WM_NCMOUSELEAVE,                   // 674
    mouse_leave = WM_MOUSELEAVE,                        // 675
    dpi_scale_changed = WM_DPICHANGED,                  // 736
    render_format = WM_RENDERFORMAT,                    // 773
    render_all_formats = WM_RENDERALLFORMATS,           // 774
    destroy_clipboard = WM_DESTROYCLIPBOARD,            // 775
    draw_clipboard = WM_DRAWCLIPBOARD,                  // 776
    paint_clipboard = WM_PAINTCLIPBOARD,                // 777
    vscroll_clipboard = WM_VSCROLLCLIPBOARD,            // 778
    size_clipboard = WM_SIZECLIPBOARD,                  // 779
    ask_cb_format_name = WM_ASKCBFORMATNAME,            // 780
    change_cb_chain = WM_CHANGECBCHAIN,                 // 781
    hscroll_clipboard = WM_HSCROLLCLIPBOARD,            // 782
    app_command = WM_APPCOMMAND,                        // 793
    theme_changed = WM_THEMECHANGED,                    // 794
    clipboard_update = WM_CLIPBOARDUPDATE,              // 797
    user = WM_USER                                      // 1024
};

/**
 * @brief Converts a windows_messages enum value to its underlying uint32_t value, which corresponds
 * to the actual Windows message constant defined in the Windows API.
 *
 * @param message The windows_messages enum value to be converted to its underlying uint32_t value.
 * @return std::uint32_t The underlying uint32_t value corresponding to the given windows_messages
 * enum value.
 */
constexpr std::uint32_t to_underlying(const windows_messages message) noexcept {
    return static_cast<std::uint32_t>(message);
}

/**
 * @brief Converts a uint32_t value corresponding to a Windows message constant defined in the
 * Windows API to its corresponding windows_messages enum value.
 *
 * @param message The uint32_t value corresponding to a Windows message constant defined in the
 * Windows API.
 * @return windows_messages The corresponding windows_messages enum value.
 */
constexpr windows_messages to_windows_message(const std::uint32_t message) noexcept {
    return static_cast<windows_messages>(message);
}

} // namespace tca::engine::platform::win32
