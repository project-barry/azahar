// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <fmt/format.h>

// Test instrumentation (project-barry, AYN Thor): "barry-touch:" log lines
// for each touch Qt delivers, each press the touchscreen bounds check
// refuses and each press/release the 3DS HID samples, stamped with the
// local wall clock (HH:MM:SS.mmm) to match a recording of the panel.
namespace BarryTouch {

inline std::string Now() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() %
        1000;
    return fmt::format("{:02}:{:02}:{:02}.{:03}", tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
}

} // namespace BarryTouch
