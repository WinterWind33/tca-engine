// Copyright (c) 2026 Andrea Ballestrazzi
// SPDX-License-Identifier: MIT
#include "platform/win32/win32-keyboard.hpp"

// C++ STL
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

// Third-party
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

// Engine
#include "input/key.hpp"

// Utilities
#include "utilities/win32/keyboard-messages.hpp"

namespace {

namespace epw32n = tca::engine::platform::win32;
using epw32n::scan_code;
using tca::engine::input::key;
using tests::utilities::make_key_lparam;

} // namespace

TEST_CASE("scan_code_from_lparam", "[tca][engine][platform][win32][win32_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(std::is_same_v<decltype(epw32n::scan_code_from_lparam(
                                          std::declval<epw32n::details::lparam_t>())),
                                      scan_code>);
        STATIC_REQUIRE(
            noexcept(epw32n::scan_code_from_lparam(std::declval<epw32n::details::lparam_t>())));
    }

    GIVEN("The LPARAM of a key that isn't extended") {
        THEN("The scan code is bits 16-23 alone") {
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0x1C)) == 0x001C);
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0x39)) == 0x0039);
        }
    }

    GIVEN("The LPARAM of an extended key (bit 24 set)") {
        THEN("The scan code carries the 0xE0 prefix in its high byte") {
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0xE01C)) == 0xE01C);
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0xE01D)) == 0xE01D);
        }
    }

    GIVEN("The other bits of the LPARAM set (repeat count, context, previous state, transition)") {
        THEN("They don't leak into the scan code") {
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0x1E, true, true, true)) ==
                    0x001E);
            REQUIRE(epw32n::scan_code_from_lparam(make_key_lparam(0xE038, true, true, true)) ==
                    0xE038);

            // Repeat count at its maximum.
            const auto lparam{static_cast<epw32n::details::lparam_t>(
                static_cast<std::uint32_t>(make_key_lparam(0x1E)) | 0xFFFFU)};
            REQUIRE(epw32n::scan_code_from_lparam(lparam) == 0x001E);
        }
    }

    GIVEN("A key-up LPARAM as Windows passes it on 64-bit (zero-extended, bit 31 set)") {
        const auto lparam{static_cast<epw32n::details::lparam_t>(0x00000000C0390001ULL)};

        THEN("The scan code is still read from bits 16-24") {
            REQUIRE(epw32n::scan_code_from_lparam(lparam) == 0x0039);
        }
    }
}

TEST_CASE("key_from_scan_code", "[tca][engine][platform][win32][win32_keyboard][input]") {
    SECTION("Type traits") {
        STATIC_REQUIRE(
            std::is_same_v<decltype(epw32n::key_from_scan_code(std::declval<scan_code>())),
                           std::optional<key>>);
        STATIC_REQUIRE(noexcept(epw32n::key_from_scan_code(std::declval<scan_code>())));
    }

    SECTION("Physical positions, whatever the layout") {
        // Scan codes are positions: 0x10 is "where Q is on US QWERTY", which types 'a' on AZERTY.
        const auto [scanCode, expected] = GENERATE(table<scan_code, key>({
            // Letters, by row
            {0x10,   key::q              },
            {0x11,   key::w              },
            {0x12,   key::e              },
            {0x13,   key::r              },
            {0x14,   key::t              },
            {0x15,   key::y              },
            {0x16,   key::u              },
            {0x17,   key::i              },
            {0x18,   key::o              },
            {0x19,   key::p              },
            {0x1E,   key::a              },
            {0x1F,   key::s              },
            {0x20,   key::d              },
            {0x21,   key::f              },
            {0x22,   key::g              },
            {0x23,   key::h              },
            {0x24,   key::j              },
            {0x25,   key::k              },
            {0x26,   key::l              },
            {0x2C,   key::z              },
            {0x2D,   key::x              },
            {0x2E,   key::c              },
            {0x2F,   key::v              },
            {0x30,   key::b              },
            {0x31,   key::n              },
            {0x32,   key::m              },

            // Digit row
            {0x02,   key::digit_1        },
            {0x03,   key::digit_2        },
            {0x04,   key::digit_3        },
            {0x05,   key::digit_4        },
            {0x06,   key::digit_5        },
            {0x07,   key::digit_6        },
            {0x08,   key::digit_7        },
            {0x09,   key::digit_8        },
            {0x0A,   key::digit_9        },
            {0x0B,   key::digit_0        },

            // Punctuation
            {0x29,   key::backquote      },
            {0x0C,   key::minus          },
            {0x0D,   key::equal          },
            {0x1A,   key::bracket_left   },
            {0x1B,   key::bracket_right  },
            {0x2B,   key::backslash      },
            {0x27,   key::semicolon      },
            {0x28,   key::quote          },
            {0x33,   key::comma          },
            {0x34,   key::period         },
            {0x35,   key::slash          },

            // ISO, JIS, Korean and ABNT extras
            {0x56,   key::intl_backslash },
            {0x73,   key::intl_ro        },
            {0x7D,   key::intl_yen       },
            {0x79,   key::convert        },
            {0x7B,   key::non_convert    },
            {0x70,   key::kana_mode      },
            {0x72,   key::lang_1         },
            {0x71,   key::lang_2         },

            // Editing and modifiers
            {0x0E,   key::backspace      },
            {0x0F,   key::tab            },
            {0x1C,   key::enter          },
            {0x39,   key::space          },
            {0x3A,   key::caps_lock      },
            {0x2A,   key::shift_left     },
            {0x36,   key::shift_right    },
            {0x1D,   key::control_left   },
            {0xE01D, key::control_right  },
            {0x38,   key::alt_left       },
            {0xE038, key::alt_right      },
            {0xE05B, key::meta_left      },
            {0xE05C, key::meta_right     },
            {0xE05D, key::context_menu   },

            // Control pad and arrows: the extended twins of the numpad keys
            {0xE052, key::insert         },
            {0xE053, key::delete_key     },
            {0xE047, key::home           },
            {0xE04F, key::end            },
            {0xE049, key::page_up        },
            {0xE051, key::page_down      },
            {0xE048, key::arrow_up       },
            {0xE050, key::arrow_down     },
            {0xE04B, key::arrow_left     },
            {0xE04D, key::arrow_right    },

            // Numpad
            {0xE045, key::num_lock       },
            {0x52,   key::numpad_0       },
            {0x4F,   key::numpad_1       },
            {0x50,   key::numpad_2       },
            {0x51,   key::numpad_3       },
            {0x4B,   key::numpad_4       },
            {0x4C,   key::numpad_5       },
            {0x4D,   key::numpad_6       },
            {0x47,   key::numpad_7       },
            {0x48,   key::numpad_8       },
            {0x49,   key::numpad_9       },
            {0x4E,   key::numpad_add     },
            {0x4A,   key::numpad_subtract},
            {0x37,   key::numpad_multiply},
            {0xE035, key::numpad_divide  },
            {0x53,   key::numpad_decimal },
            {0xE01C, key::numpad_enter   },
            {0x7E,   key::numpad_comma   },
            {0x59,   key::numpad_equal   },

            // Function row
            {0x01,   key::escape         },
            {0x3B,   key::f1             },
            {0x3C,   key::f2             },
            {0x3D,   key::f3             },
            {0x3E,   key::f4             },
            {0x3F,   key::f5             },
            {0x40,   key::f6             },
            {0x41,   key::f7             },
            {0x42,   key::f8             },
            {0x43,   key::f9             },
            {0x44,   key::f10            },
            {0x57,   key::f11            },
            {0x58,   key::f12            },
            {0x64,   key::f13            },
            {0x65,   key::f14            },
            {0x66,   key::f15            },
            {0x67,   key::f16            },
            {0x68,   key::f17            },
            {0x69,   key::f18            },
            {0x6A,   key::f19            },
            {0x6B,   key::f20            },
            {0x6C,   key::f21            },
            {0x6D,   key::f22            },
            {0x6E,   key::f23            },
            {0x76,   key::f24            },
            {0xE037, key::print_screen   },
            {0x46,   key::scroll_lock    },
            {0x45,   key::pause          },
        }));

        CAPTURE(scanCode);
        REQUIRE(epw32n::key_from_scan_code(scanCode) == expected);
    }

    SECTION("The extended bit tells twins apart") {
        // Same low byte, different key: what makes numpad Enter, right Ctrl/Alt and the control
        // pad distinct from their twins whatever Num Lock says.
        REQUIRE(epw32n::key_from_scan_code(0x001C) == key::enter);
        REQUIRE(epw32n::key_from_scan_code(0xE01C) == key::numpad_enter);
        REQUIRE(epw32n::key_from_scan_code(0x001D) == key::control_left);
        REQUIRE(epw32n::key_from_scan_code(0xE01D) == key::control_right);
        REQUIRE(epw32n::key_from_scan_code(0x0038) == key::alt_left);
        REQUIRE(epw32n::key_from_scan_code(0xE038) == key::alt_right);
        REQUIRE(epw32n::key_from_scan_code(0x0035) == key::slash);
        REQUIRE(epw32n::key_from_scan_code(0xE035) == key::numpad_divide);
        REQUIRE(epw32n::key_from_scan_code(0x0048) == key::numpad_8);
        REQUIRE(epw32n::key_from_scan_code(0xE048) == key::arrow_up);
        REQUIRE(epw32n::key_from_scan_code(0x0053) == key::numpad_decimal);
        REQUIRE(epw32n::key_from_scan_code(0xE053) == key::delete_key);
        REQUIRE(epw32n::key_from_scan_code(0x0045) == key::pause);
        REQUIRE(epw32n::key_from_scan_code(0xE045) == key::num_lock);
    }

    SECTION("Alternate scan codes Windows reports for a key") {
        // Alt+Print Screen reports SysRq's scan code.
        REQUIRE(epw32n::key_from_scan_code(0x0054) == key::print_screen);
        // Ctrl+Pause reports Break's scan code.
        REQUIRE(epw32n::key_from_scan_code(0xE046) == key::pause);
        // Some CJK IMEs set the extended bit on right Shift.
        REQUIRE(epw32n::key_from_scan_code(0xE036) == key::shift_right);
    }

    SECTION("Unmapped scan codes give nothing, never a guess") {
        const auto scanCode = GENERATE(scan_code{0x0000}, scan_code{0x007F}, scan_code{0x00FF},
                                       scan_code{0xE000}, scan_code{0xE0FF}, scan_code{0xE11D});

        CAPTURE(scanCode);
        REQUIRE_FALSE(epw32n::key_from_scan_code(scanCode).has_value());
    }

    SECTION("Every key is reachable from some scan code") {
        std::array<bool, tca::engine::input::key_count> reached{};
        for (std::uint32_t low{}; low <= 0xFFU; ++low) {
            for (const std::uint32_t prefix : {0x0000U, 0xE000U}) {
                if (const auto k{
                        epw32n::key_from_scan_code(static_cast<scan_code>(prefix | low))}) {
                    reached[tca::engine::input::key_index(*k)] = true;
                }
            }
        }

        for (std::size_t i{}; i < tca::engine::input::key_count; ++i) {
            CAPTURE(tca::engine::input::key_name(static_cast<key>(i)));
            CHECK(reached[i]);
        }
    }
}
