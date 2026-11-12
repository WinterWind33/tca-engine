// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/linux/linux-keyboard.hpp"

// Third-party
#include <GLFW/glfw3.h>
#include <linux/input-event-codes.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// C++ STL
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

// Engine
#include "input/key.hpp"

namespace {

namespace eplxn = tca::engine::platform::posix_linux;
namespace ein = tca::engine::input;
using ein::key;
using eplxn::evdev_code;

// X11 keycodes are evdev codes shifted by 8.
constexpr int x11_keycode_offset{8};

// Keys whose position GLFW has no token for: they only arrive as GLFW_KEY_UNKNOWN plus a scancode.
constexpr std::array keys_without_glfw_token{
    key::intl_ro,   key::intl_yen, key::convert, key::non_convert,
    key::kana_mode, key::lang_1,   key::lang_2,  key::numpad_comma,
};

[[nodiscard]] constexpr bool has_no_glfw_token(const key k) noexcept {
    for (const key tokenless : keys_without_glfw_token) {
        if (tokenless == k) {
            return true;
        }
    }

    return false;
}

} // namespace

TEST_CASE("key_from_glfw_key", "[tca][engine][platform][linux][linux_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(std::is_same_v<decltype(eplxn::key_from_glfw_key(std::declval<int>())),
                                      std::optional<key>>);
        STATIC_REQUIRE(noexcept(eplxn::key_from_glfw_key(std::declval<int>())));
    }

    SECTION("Every GLFW key token with a position maps to it") {
        // GLFW tokens are physical: GLFW_KEY_Q is "where Q is on US QWERTY", which types 'a' on
        // AZERTY.
        const auto [glfwKey, expected] = GENERATE(table<int, key>({
            // Letters
            {GLFW_KEY_A,             key::a              },
            {GLFW_KEY_B,             key::b              },
            {GLFW_KEY_C,             key::c              },
            {GLFW_KEY_D,             key::d              },
            {GLFW_KEY_E,             key::e              },
            {GLFW_KEY_F,             key::f              },
            {GLFW_KEY_G,             key::g              },
            {GLFW_KEY_H,             key::h              },
            {GLFW_KEY_I,             key::i              },
            {GLFW_KEY_J,             key::j              },
            {GLFW_KEY_K,             key::k              },
            {GLFW_KEY_L,             key::l              },
            {GLFW_KEY_M,             key::m              },
            {GLFW_KEY_N,             key::n              },
            {GLFW_KEY_O,             key::o              },
            {GLFW_KEY_P,             key::p              },
            {GLFW_KEY_Q,             key::q              },
            {GLFW_KEY_R,             key::r              },
            {GLFW_KEY_S,             key::s              },
            {GLFW_KEY_T,             key::t              },
            {GLFW_KEY_U,             key::u              },
            {GLFW_KEY_V,             key::v              },
            {GLFW_KEY_W,             key::w              },
            {GLFW_KEY_X,             key::x              },
            {GLFW_KEY_Y,             key::y              },
            {GLFW_KEY_Z,             key::z              },

            // Digit row
            {GLFW_KEY_0,             key::digit_0        },
            {GLFW_KEY_1,             key::digit_1        },
            {GLFW_KEY_2,             key::digit_2        },
            {GLFW_KEY_3,             key::digit_3        },
            {GLFW_KEY_4,             key::digit_4        },
            {GLFW_KEY_5,             key::digit_5        },
            {GLFW_KEY_6,             key::digit_6        },
            {GLFW_KEY_7,             key::digit_7        },
            {GLFW_KEY_8,             key::digit_8        },
            {GLFW_KEY_9,             key::digit_9        },

            // Punctuation
            {GLFW_KEY_GRAVE_ACCENT,  key::backquote      },
            {GLFW_KEY_MINUS,         key::minus          },
            {GLFW_KEY_EQUAL,         key::equal          },
            {GLFW_KEY_LEFT_BRACKET,  key::bracket_left   },
            {GLFW_KEY_RIGHT_BRACKET, key::bracket_right  },
            {GLFW_KEY_BACKSLASH,     key::backslash      },
            {GLFW_KEY_SEMICOLON,     key::semicolon      },
            {GLFW_KEY_APOSTROPHE,    key::quote          },
            {GLFW_KEY_COMMA,         key::comma          },
            {GLFW_KEY_PERIOD,        key::period         },
            {GLFW_KEY_SLASH,         key::slash          },

            // The ISO key: X11 calls it WORLD_1, Wayland WORLD_2
            {GLFW_KEY_WORLD_1,       key::intl_backslash },
            {GLFW_KEY_WORLD_2,       key::intl_backslash },

            // Editing and modifiers
            {GLFW_KEY_BACKSPACE,     key::backspace      },
            {GLFW_KEY_TAB,           key::tab            },
            {GLFW_KEY_ENTER,         key::enter          },
            {GLFW_KEY_SPACE,         key::space          },
            {GLFW_KEY_CAPS_LOCK,     key::caps_lock      },
            {GLFW_KEY_LEFT_SHIFT,    key::shift_left     },
            {GLFW_KEY_RIGHT_SHIFT,   key::shift_right    },
            {GLFW_KEY_LEFT_CONTROL,  key::control_left   },
            {GLFW_KEY_RIGHT_CONTROL, key::control_right  },
            {GLFW_KEY_LEFT_ALT,      key::alt_left       },
            {GLFW_KEY_RIGHT_ALT,     key::alt_right      },
            {GLFW_KEY_LEFT_SUPER,    key::meta_left      },
            {GLFW_KEY_RIGHT_SUPER,   key::meta_right     },
            {GLFW_KEY_MENU,          key::context_menu   },

            // Control pad and arrows
            {GLFW_KEY_INSERT,        key::insert         },
            {GLFW_KEY_DELETE,        key::delete_key     },
            {GLFW_KEY_HOME,          key::home           },
            {GLFW_KEY_END,           key::end            },
            {GLFW_KEY_PAGE_UP,       key::page_up        },
            {GLFW_KEY_PAGE_DOWN,     key::page_down      },
            {GLFW_KEY_UP,            key::arrow_up       },
            {GLFW_KEY_DOWN,          key::arrow_down     },
            {GLFW_KEY_LEFT,          key::arrow_left     },
            {GLFW_KEY_RIGHT,         key::arrow_right    },

            // Numpad
            {GLFW_KEY_NUM_LOCK,      key::num_lock       },
            {GLFW_KEY_KP_0,          key::numpad_0       },
            {GLFW_KEY_KP_1,          key::numpad_1       },
            {GLFW_KEY_KP_2,          key::numpad_2       },
            {GLFW_KEY_KP_3,          key::numpad_3       },
            {GLFW_KEY_KP_4,          key::numpad_4       },
            {GLFW_KEY_KP_5,          key::numpad_5       },
            {GLFW_KEY_KP_6,          key::numpad_6       },
            {GLFW_KEY_KP_7,          key::numpad_7       },
            {GLFW_KEY_KP_8,          key::numpad_8       },
            {GLFW_KEY_KP_9,          key::numpad_9       },
            {GLFW_KEY_KP_ADD,        key::numpad_add     },
            {GLFW_KEY_KP_SUBTRACT,   key::numpad_subtract},
            {GLFW_KEY_KP_MULTIPLY,   key::numpad_multiply},
            {GLFW_KEY_KP_DIVIDE,     key::numpad_divide  },
            {GLFW_KEY_KP_DECIMAL,    key::numpad_decimal },
            {GLFW_KEY_KP_ENTER,      key::numpad_enter   },
            {GLFW_KEY_KP_EQUAL,      key::numpad_equal   },

            // Function row
            {GLFW_KEY_ESCAPE,        key::escape         },
            {GLFW_KEY_F1,            key::f1             },
            {GLFW_KEY_F2,            key::f2             },
            {GLFW_KEY_F3,            key::f3             },
            {GLFW_KEY_F4,            key::f4             },
            {GLFW_KEY_F5,            key::f5             },
            {GLFW_KEY_F6,            key::f6             },
            {GLFW_KEY_F7,            key::f7             },
            {GLFW_KEY_F8,            key::f8             },
            {GLFW_KEY_F9,            key::f9             },
            {GLFW_KEY_F10,           key::f10            },
            {GLFW_KEY_F11,           key::f11            },
            {GLFW_KEY_F12,           key::f12            },
            {GLFW_KEY_F13,           key::f13            },
            {GLFW_KEY_F14,           key::f14            },
            {GLFW_KEY_F15,           key::f15            },
            {GLFW_KEY_F16,           key::f16            },
            {GLFW_KEY_F17,           key::f17            },
            {GLFW_KEY_F18,           key::f18            },
            {GLFW_KEY_F19,           key::f19            },
            {GLFW_KEY_F20,           key::f20            },
            {GLFW_KEY_F21,           key::f21            },
            {GLFW_KEY_F22,           key::f22            },
            {GLFW_KEY_F23,           key::f23            },
            {GLFW_KEY_F24,           key::f24            },
            {GLFW_KEY_PRINT_SCREEN,  key::print_screen   },
            {GLFW_KEY_SCROLL_LOCK,   key::scroll_lock    },
            {GLFW_KEY_PAUSE,         key::pause          },
        }));

        CAPTURE(glfwKey, ein::key_name(expected));
        REQUIRE(eplxn::key_from_glfw_key(glfwKey) == expected);
    }

    SECTION("Twins are distinct tokens, so they stay distinct keys") {
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_ENTER) == key::enter);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_KP_ENTER) == key::numpad_enter);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_UP) == key::arrow_up);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_KP_8) == key::numpad_8);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_DELETE) == key::delete_key);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_KP_DECIMAL) == key::numpad_decimal);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_SLASH) == key::slash);
        REQUIRE(eplxn::key_from_glfw_key(GLFW_KEY_KP_DIVIDE) == key::numpad_divide);
    }

    SECTION("Tokens with no position and values that are no token give nothing") {
        const auto glfwKey = GENERATE(GLFW_KEY_UNKNOWN, GLFW_KEY_F25, 0, 1, 31, 127, 160, 255,
                                      GLFW_KEY_LAST + 1, -2, 100000);

        CAPTURE(glfwKey);
        REQUIRE_FALSE(eplxn::key_from_glfw_key(glfwKey).has_value());
    }

    SECTION("Every key but the ones GLFW has no token for is reachable from some token") {
        std::array<bool, ein::key_count> reached{};
        for (int glfwKey{GLFW_KEY_UNKNOWN}; glfwKey <= GLFW_KEY_LAST; ++glfwKey) {
            if (const auto k{eplxn::key_from_glfw_key(glfwKey)}) {
                reached[ein::key_index(*k)] = true;
            }
        }

        for (std::size_t i{}; i < ein::key_count; ++i) {
            const auto k{static_cast<key>(i)};
            CAPTURE(ein::key_name(k));
            CHECK(reached[i] == !has_no_glfw_token(k));
        }
    }
}

TEST_CASE("key_from_evdev_code", "[tca][engine][platform][linux][linux_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(
            std::is_same_v<decltype(eplxn::key_from_evdev_code(std::declval<evdev_code>())),
                           std::optional<key>>);
        STATIC_REQUIRE(noexcept(eplxn::key_from_evdev_code(std::declval<evdev_code>())));
    }

    SECTION("Physical positions, whatever the layout") {
        const auto [code, expected] = GENERATE(table<evdev_code, key>({
            // Letters, by row
            {KEY_Q,                key::q              },
            {KEY_W,                key::w              },
            {KEY_E,                key::e              },
            {KEY_R,                key::r              },
            {KEY_T,                key::t              },
            {KEY_Y,                key::y              },
            {KEY_U,                key::u              },
            {KEY_I,                key::i              },
            {KEY_O,                key::o              },
            {KEY_P,                key::p              },
            {KEY_A,                key::a              },
            {KEY_S,                key::s              },
            {KEY_D,                key::d              },
            {KEY_F,                key::f              },
            {KEY_G,                key::g              },
            {KEY_H,                key::h              },
            {KEY_J,                key::j              },
            {KEY_K,                key::k              },
            {KEY_L,                key::l              },
            {KEY_Z,                key::z              },
            {KEY_X,                key::x              },
            {KEY_C,                key::c              },
            {KEY_V,                key::v              },
            {KEY_B,                key::b              },
            {KEY_N,                key::n              },
            {KEY_M,                key::m              },

            // Digit row
            {KEY_1,                key::digit_1        },
            {KEY_2,                key::digit_2        },
            {KEY_3,                key::digit_3        },
            {KEY_4,                key::digit_4        },
            {KEY_5,                key::digit_5        },
            {KEY_6,                key::digit_6        },
            {KEY_7,                key::digit_7        },
            {KEY_8,                key::digit_8        },
            {KEY_9,                key::digit_9        },
            {KEY_0,                key::digit_0        },

            // Punctuation
            {KEY_GRAVE,            key::backquote      },
            {KEY_MINUS,            key::minus          },
            {KEY_EQUAL,            key::equal          },
            {KEY_LEFTBRACE,        key::bracket_left   },
            {KEY_RIGHTBRACE,       key::bracket_right  },
            {KEY_BACKSLASH,        key::backslash      },
            {KEY_SEMICOLON,        key::semicolon      },
            {KEY_APOSTROPHE,       key::quote          },
            {KEY_COMMA,            key::comma          },
            {KEY_DOT,              key::period         },
            {KEY_SLASH,            key::slash          },

            // ISO, JIS, Korean and ABNT extras: the keys GLFW has no token for
            {KEY_102ND,            key::intl_backslash },
            {KEY_RO,               key::intl_ro        },
            {KEY_YEN,              key::intl_yen       },
            {KEY_HENKAN,           key::convert        },
            {KEY_MUHENKAN,         key::non_convert    },
            {KEY_KATAKANAHIRAGANA, key::kana_mode      },
            {KEY_HANGEUL,          key::lang_1         },
            {KEY_HANJA,            key::lang_2         },

            // Editing and modifiers
            {KEY_BACKSPACE,        key::backspace      },
            {KEY_TAB,              key::tab            },
            {KEY_ENTER,            key::enter          },
            {KEY_SPACE,            key::space          },
            {KEY_CAPSLOCK,         key::caps_lock      },
            {KEY_LEFTSHIFT,        key::shift_left     },
            {KEY_RIGHTSHIFT,       key::shift_right    },
            {KEY_LEFTCTRL,         key::control_left   },
            {KEY_RIGHTCTRL,        key::control_right  },
            {KEY_LEFTALT,          key::alt_left       },
            {KEY_RIGHTALT,         key::alt_right      },
            {KEY_LEFTMETA,         key::meta_left      },
            {KEY_RIGHTMETA,        key::meta_right     },
            // evdev calls the PC Menu key "Compose"
            {KEY_COMPOSE,          key::context_menu   },

            // Control pad and arrows
            {KEY_INSERT,           key::insert         },
            {KEY_DELETE,           key::delete_key     },
            {KEY_HOME,             key::home           },
            {KEY_END,              key::end            },
            {KEY_PAGEUP,           key::page_up        },
            {KEY_PAGEDOWN,         key::page_down      },
            {KEY_UP,               key::arrow_up       },
            {KEY_DOWN,             key::arrow_down     },
            {KEY_LEFT,             key::arrow_left     },
            {KEY_RIGHT,            key::arrow_right    },

            // Numpad
            {KEY_NUMLOCK,          key::num_lock       },
            {KEY_KP0,              key::numpad_0       },
            {KEY_KP1,              key::numpad_1       },
            {KEY_KP2,              key::numpad_2       },
            {KEY_KP3,              key::numpad_3       },
            {KEY_KP4,              key::numpad_4       },
            {KEY_KP5,              key::numpad_5       },
            {KEY_KP6,              key::numpad_6       },
            {KEY_KP7,              key::numpad_7       },
            {KEY_KP8,              key::numpad_8       },
            {KEY_KP9,              key::numpad_9       },
            {KEY_KPPLUS,           key::numpad_add     },
            {KEY_KPMINUS,          key::numpad_subtract},
            {KEY_KPASTERISK,       key::numpad_multiply},
            {KEY_KPSLASH,          key::numpad_divide  },
            {KEY_KPDOT,            key::numpad_decimal },
            {KEY_KPENTER,          key::numpad_enter   },
            {KEY_KPCOMMA,          key::numpad_comma   },
            {KEY_KPEQUAL,          key::numpad_equal   },

            // Function row
            {KEY_ESC,              key::escape         },
            {KEY_F1,               key::f1             },
            {KEY_F2,               key::f2             },
            {KEY_F3,               key::f3             },
            {KEY_F4,               key::f4             },
            {KEY_F5,               key::f5             },
            {KEY_F6,               key::f6             },
            {KEY_F7,               key::f7             },
            {KEY_F8,               key::f8             },
            {KEY_F9,               key::f9             },
            {KEY_F10,              key::f10            },
            {KEY_F11,              key::f11            },
            {KEY_F12,              key::f12            },
            {KEY_F13,              key::f13            },
            {KEY_F14,              key::f14            },
            {KEY_F15,              key::f15            },
            {KEY_F16,              key::f16            },
            {KEY_F17,              key::f17            },
            {KEY_F18,              key::f18            },
            {KEY_F19,              key::f19            },
            {KEY_F20,              key::f20            },
            {KEY_F21,              key::f21            },
            {KEY_F22,              key::f22            },
            {KEY_F23,              key::f23            },
            {KEY_F24,              key::f24            },
            {KEY_SYSRQ,            key::print_screen   },
            {KEY_SCROLLLOCK,       key::scroll_lock    },
            {KEY_PAUSE,            key::pause          },
        }));

        CAPTURE(code, ein::key_name(expected));
        REQUIRE(eplxn::key_from_evdev_code(code) == expected);
    }

    SECTION("Codes that are not keyboard positions give nothing, never a guess") {
        const auto code =
            GENERATE(evdev_code{KEY_RESERVED}, evdev_code{KEY_MUTE}, evdev_code{KEY_VOLUMEUP},
                     evdev_code{KEY_POWER}, evdev_code{KEY_MENU}, evdev_code{BTN_LEFT},
                     evdev_code{KEY_MAX}, evdev_code{0xFFFF});

        CAPTURE(code);
        REQUIRE_FALSE(eplxn::key_from_evdev_code(code).has_value());
    }

    SECTION("Every key is reachable from some evdev code") {
        std::array<bool, ein::key_count> reached{};
        for (std::uint32_t code{}; code <= KEY_MAX; ++code) {
            if (const auto k{eplxn::key_from_evdev_code(static_cast<evdev_code>(code))}) {
                reached[ein::key_index(*k)] = true;
            }
        }

        for (std::size_t i{}; i < ein::key_count; ++i) {
            CAPTURE(ein::key_name(static_cast<key>(i)));
            CHECK(reached[i]);
        }
    }
}

TEST_CASE("evdev_code_from_glfw_scancode",
          "[tca][engine][platform][linux][linux_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(std::is_same_v<decltype(eplxn::evdev_code_from_glfw_scancode(
                                          std::declval<int>(), std::declval<int>())),
                                      std::optional<evdev_code>>);
        STATIC_REQUIRE(noexcept(
            eplxn::evdev_code_from_glfw_scancode(std::declval<int>(), std::declval<int>())));
    }

    GIVEN("The X11 platform") {
        THEN("The scancode is the X keycode, the evdev code plus 8") {
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_RO + x11_keycode_offset,
                                                         GLFW_PLATFORM_X11) == KEY_RO);
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_ESC + x11_keycode_offset,
                                                         GLFW_PLATFORM_X11) == KEY_ESC);
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_HANGEUL + x11_keycode_offset,
                                                         GLFW_PLATFORM_X11) == KEY_HANGEUL);
        }

        THEN("Keycodes below 8 are not evdev codes") {
            const auto scancode = GENERATE(-1, 0, 1, 7);

            CAPTURE(scancode);
            REQUIRE_FALSE(
                eplxn::evdev_code_from_glfw_scancode(scancode, GLFW_PLATFORM_X11).has_value());
        }
    }

    GIVEN("The Wayland platform") {
        THEN("The scancode is the evdev code") {
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_RO, GLFW_PLATFORM_WAYLAND) == KEY_RO);
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_ESC, GLFW_PLATFORM_WAYLAND) ==
                    KEY_ESC);
            REQUIRE(eplxn::evdev_code_from_glfw_scancode(KEY_HANGEUL, GLFW_PLATFORM_WAYLAND) ==
                    KEY_HANGEUL);
        }

        THEN("Negative scancodes are not evdev codes") {
            REQUIRE_FALSE(
                eplxn::evdev_code_from_glfw_scancode(-1, GLFW_PLATFORM_WAYLAND).has_value());
        }
    }

    GIVEN("Either platform") {
        const auto [glfwPlatform, offset] = GENERATE(table<int, int>({
            {GLFW_PLATFORM_X11,     x11_keycode_offset},
            {GLFW_PLATFORM_WAYLAND, 0                 },
        }));

        THEN("A scancode beyond what an evdev code can hold gives nothing") {
            CAPTURE(glfwPlatform);
            REQUIRE_FALSE(
                eplxn::evdev_code_from_glfw_scancode(0x10000 + offset, glfwPlatform).has_value());
        }
    }

    GIVEN("A platform whose scancodes are not evdev codes") {
        const auto glfwPlatform =
            GENERATE(GLFW_PLATFORM_NULL, GLFW_PLATFORM_WIN32, GLFW_PLATFORM_COCOA, 0);

        THEN("No scancode has an evdev code") {
            CAPTURE(glfwPlatform);
            REQUIRE_FALSE(eplxn::evdev_code_from_glfw_scancode(KEY_RO, glfwPlatform).has_value());
            REQUIRE_FALSE(
                eplxn::evdev_code_from_glfw_scancode(KEY_RO + x11_keycode_offset, glfwPlatform)
                    .has_value());
        }
    }
}

TEST_CASE("key_from_glfw_key_event", "[tca][engine][platform][linux][linux_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(
            std::is_same_v<decltype(eplxn::key_from_glfw_key_event(
                               std::declval<int>(), std::declval<int>(), std::declval<int>())),
                           std::optional<key>>);
        STATIC_REQUIRE(noexcept(eplxn::key_from_glfw_key_event(
            std::declval<int>(), std::declval<int>(), std::declval<int>())));
    }

    SECTION("A token with a position wins over the scancode") {
        // Deliberately mismatched: the scancode says Escape, the token says Q.
        const auto [glfwPlatform, scancode] = GENERATE(table<int, int>({
            {GLFW_PLATFORM_X11,     KEY_ESC + x11_keycode_offset},
            {GLFW_PLATFORM_WAYLAND, KEY_ESC                     },
            {GLFW_PLATFORM_NULL,    KEY_ESC                     },
        }));

        CAPTURE(glfwPlatform);
        REQUIRE(eplxn::key_from_glfw_key_event(GLFW_KEY_Q, scancode, glfwPlatform) == key::q);
    }

    SECTION("GLFW_KEY_UNKNOWN falls back to the scancode's evdev code") {
        const auto [code, expected] = GENERATE(table<evdev_code, key>({
            {KEY_RO,               key::intl_ro     },
            {KEY_YEN,              key::intl_yen    },
            {KEY_HENKAN,           key::convert     },
            {KEY_MUHENKAN,         key::non_convert },
            {KEY_KATAKANAHIRAGANA, key::kana_mode   },
            {KEY_HANGEUL,          key::lang_1      },
            {KEY_HANJA,            key::lang_2      },
            {KEY_KPCOMMA,          key::numpad_comma},
        }));

        CAPTURE(code, ein::key_name(expected));

        SECTION("On X11") {
            REQUIRE(eplxn::key_from_glfw_key_event(GLFW_KEY_UNKNOWN, code + x11_keycode_offset,
                                                   GLFW_PLATFORM_X11) == expected);
        }

        SECTION("On Wayland") {
            REQUIRE(eplxn::key_from_glfw_key_event(GLFW_KEY_UNKNOWN, code, GLFW_PLATFORM_WAYLAND) ==
                    expected);
        }
    }

    SECTION("A token with no position falls back to the scancode too") {
        REQUIRE(eplxn::key_from_glfw_key_event(GLFW_KEY_F25, KEY_RO, GLFW_PLATFORM_WAYLAND) ==
                key::intl_ro);
    }

    SECTION("GLFW_KEY_UNKNOWN on a platform whose scancodes are not evdev codes gives nothing") {
        REQUIRE_FALSE(eplxn::key_from_glfw_key_event(GLFW_KEY_UNKNOWN, KEY_RO, GLFW_PLATFORM_NULL)
                          .has_value());
    }

    SECTION("Neither the token nor the scancode mapping to a key gives nothing") {
        const auto [glfwPlatform, scancode] = GENERATE(table<int, int>({
            {GLFW_PLATFORM_X11,     KEY_MUTE + x11_keycode_offset},
            {GLFW_PLATFORM_X11,     0                            },
            {GLFW_PLATFORM_WAYLAND, KEY_MUTE                     },
            {GLFW_PLATFORM_WAYLAND, -1                           },
        }));

        CAPTURE(glfwPlatform, scancode);
        REQUIRE_FALSE(
            eplxn::key_from_glfw_key_event(GLFW_KEY_UNKNOWN, scancode, glfwPlatform).has_value());
    }
}
