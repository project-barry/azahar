// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include "common/common_types.h"
#include "common/logging/log.h"

// Test instrumentation (project-barry, AYN Thor): where the frame time goes
// with Separate Windows. Every 2 s the log gets one line per window (0: main,
// 1: secondary) with the time spent waiting for a free frame, in
// RenderToWindow and in CopyToSwapchain (ms, mean/max), and counts of the
// stalls: a frame recreated for a new size, a swapchain recreated for a new
// size or VSync, after a failed acquire (suboptimal/out of date/lost), after
// an out-of-date present, and SwapBuffers falling back to Scheduler::Finish.
namespace Vulkan::BarryTrace {

using Clock = std::chrono::steady_clock;

struct Timing {
    std::atomic<u64> sum_us{0};
    std::atomic<u64> max_us{0};
    std::atomic<u32> count{0};

    void Add(Clock::time_point start) {
        const u64 us = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
        sum_us += us;
        count++;
        u64 prev = max_us.load();
        while (us > prev && !max_us.compare_exchange_weak(prev, us)) {
        }
    }

    double MeanMs() const {
        const u32 n = count.load();
        return n ? sum_us.load() / 1000.0 / n : 0.0;
    }

    double MaxMs() const {
        return max_us.load() / 1000.0;
    }

    void Reset() {
        sum_us = 0;
        max_us = 0;
        count = 0;
    }
};

struct Window {
    Timing get_frame;
    Timing render_to_window;
    Timing copy_to_swapchain;
    std::atomic<u32> frame_recreated{0};
    std::atomic<u32> swapchain_size_or_vsync{0};
    std::atomic<u32> acquire_suboptimal{0};
    std::atomic<u32> acquire_out_of_date{0};
    std::atomic<u32> acquire_other{0};
    std::atomic<u32> present_out_of_date{0};

    void Reset() {
        get_frame.Reset();
        render_to_window.Reset();
        copy_to_swapchain.Reset();
        frame_recreated = 0;
        swapchain_size_or_vsync = 0;
        acquire_suboptimal = 0;
        acquire_out_of_date = 0;
        acquire_other = 0;
        present_out_of_date = 0;
    }
};

inline std::array<Window, 2> windows;
inline Timing swap_buffers;
inline std::atomic<u32> finish_fallback{0};
inline Clock::time_point reported_at{};

inline Window& For(int index) {
    return windows[std::clamp(index, 0, 1)];
}

/// Called at the end of SwapBuffers (emulation thread).
inline void MaybeReport() {
    const auto now = Clock::now();
    if (reported_at == Clock::time_point{}) {
        reported_at = now;
        return;
    }
    const double seconds = std::chrono::duration<double>(now - reported_at).count();
    if (seconds < 2.0) {
        return;
    }
    reported_at = now;
    LOG_INFO(Render_Vulkan,
             "barry-trace: {:.1f} fps, SwapBuffers {:.2f}/{:.2f} ms, Finish fallbacks {}",
             swap_buffers.count.load() / seconds, swap_buffers.MeanMs(), swap_buffers.MaxMs(),
             finish_fallback.load());
    for (int i = 0; i < 2; i++) {
        Window& w = windows[i];
        if (!w.render_to_window.count.load() && !w.copy_to_swapchain.count.load()) {
            continue;
        }
        LOG_INFO(Render_Vulkan,
                 "barry-trace: window {}: get frame {:.2f}/{:.2f} ms, render {:.2f}/{:.2f} ms, "
                 "copy {:.2f}/{:.2f} ms ({} presents); frame recreated {}, swapchain recreated "
                 "for size/vsync {}, acquire suboptimal {} out-of-date {} other {}, present "
                 "out-of-date {}",
                 i, w.get_frame.MeanMs(), w.get_frame.MaxMs(), w.render_to_window.MeanMs(),
                 w.render_to_window.MaxMs(), w.copy_to_swapchain.MeanMs(),
                 w.copy_to_swapchain.MaxMs(), w.copy_to_swapchain.count.load(),
                 w.frame_recreated.load(), w.swapchain_size_or_vsync.load(),
                 w.acquire_suboptimal.load(), w.acquire_out_of_date.load(), w.acquire_other.load(),
                 w.present_out_of_date.load());
        w.Reset();
    }
    swap_buffers.Reset();
    finish_fallback = 0;
}

} // namespace Vulkan::BarryTrace
