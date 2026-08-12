#pragma once

#include "base/ff_log.h"
#include "module/module_media.hpp"
#include "tests/media_channel_dump.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace FFMediaTest
{

class CliOptions
{
public:
    CliOptions(int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg.compare(0, 2, "--") != 0) {
                positional_.push_back(arg);
                continue;
            }

            arg.erase(0, 2);
            const std::string::size_type equal = arg.find('=');
            if (equal != std::string::npos) {
                values_[arg.substr(0, equal)] = arg.substr(equal + 1);
            } else if (i + 1 < argc && std::string(argv[i + 1]).compare(0, 2, "--") != 0) {
                values_[arg] = argv[++i];
            } else {
                values_[arg] = "1";
            }
        }
    }

    bool has(const std::string& key) const { return values_.count(key) != 0; }

    std::string get(const std::string& key, const std::string& fallback = "") const
    {
        const auto it = values_.find(key);
        return it == values_.end() ? fallback : it->second;
    }

    long getLong(const std::string& key, long fallback) const
    {
        const auto it = values_.find(key);
        if (it == values_.end())
            return fallback;
        char* end = nullptr;
        const long value = std::strtol(it->second.c_str(), &end, 0);
        return end && *end == '\0' ? value : fallback;
    }

    double getDouble(const std::string& key, double fallback) const
    {
        const auto it = values_.find(key);
        if (it == values_.end())
            return fallback;
        char* end = nullptr;
        const double value = std::strtod(it->second.c_str(), &end);
        return end && *end == '\0' ? value : fallback;
    }

    bool getBool(const std::string& key, bool fallback = false) const
    {
        if (!has(key))
            return fallback;
        const std::string value = get(key);
        return value != "0" && value != "false" && value != "off" && value != "no";
    }

    const std::vector<std::string>& positional() const { return positional_; }

private:
    std::map<std::string, std::string> values_;
    std::vector<std::string> positional_;
};

class RunMonitor
{
public:
    RunMonitor(uint64_t max_frames, double duration_sec, uint64_t report_every,
               bool verbose)
        : max_frames_(max_frames), duration_sec_(duration_sec),
          report_every_(report_every), verbose_(verbose)
    {
    }

    void reset()
    {
        stop_ = false;
        eos_ = false;
        abnormal_ = false;
        frames_ = 0;
        bytes_ = 0;
        start_ = std::chrono::steady_clock::now();
    }

    void onBuffer(const std::string& name, int queue_size,
                  const std::shared_ptr<FFMedia::MediaBuffer>& buffer)
    {
        if (!buffer)
            return;
        const uint64_t frames = ++frames_;
        bytes_.fetch_add(buffer->getActiveSize());
        if (verbose_)
            FFMedia::dumpMediaBufferBrief(name, queue_size, buffer);
        if (report_every_ && frames % report_every_ == 0) {
            const double seconds = elapsedSeconds();
            ff_info("%s: %lu buffers, %lu bytes, %.3f s, %.2f buffers/s, queue %d\n",
                    name.c_str(), frames, bytes_.load(), seconds,
                    seconds > 0.0 ? frames / seconds : 0.0, queue_size);
        }
        if (max_frames_ && frames >= max_frames_)
            requestStop();
    }

    void onStatus(const std::string& name, FFMedia::MediaStatus status)
    {
        ff_info("%s status changed to %d\n", name.c_str(), static_cast<int>(status));
        if (status == FFMedia::MediaStatus::EOS) {
            eos_ = true;
            requestStop();
        } else if (status == FFMedia::MediaStatus::ABNORMAL) {
            abnormal_ = true;
            requestStop();
        }
    }

    void wait()
    {
        std::unique_lock<std::mutex> lock(wait_mutex_);
        while (!stop_) {
            if (duration_sec_ > 0.0) {
                const double remaining = duration_sec_ - elapsedSeconds();
                if (remaining <= 0.0) {
                    stop_ = true;
                    break;
                }
                wait_condition_.wait_for(
                    lock, std::chrono::duration<double>(remaining < 0.1 ? remaining : 0.1),
                    [this] { return stop_.load(); });
            } else {
                wait_condition_.wait_for(
                    lock, std::chrono::milliseconds(100),
                    [this] { return stop_.load(); });
            }
        }
    }

    void requestStop()
    {
        stop_ = true;
        wait_condition_.notify_all();
    }
    void requestStopFromSignal() { stop_ = true; }
    bool stopped() const { return stop_; }
    bool abnormal() const { return abnormal_; }
    bool eos() const { return eos_; }
    uint64_t frames() const { return frames_; }
    uint64_t bytes() const { return bytes_; }

    double elapsedSeconds() const
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_).count();
    }

    void printSummary(const char* label) const
    {
        const double seconds = elapsedSeconds();
        ff_info("%s summary: %lu buffers, %lu bytes, %.3f s, %.2f buffers/s, eos %d, abnormal %d\n",
                label, frames_.load(), bytes_.load(), seconds,
                seconds > 0.0 ? frames_.load() / seconds : 0.0,
                eos_.load(), abnormal_.load());
    }

private:
    uint64_t max_frames_;
    double duration_sec_;
    uint64_t report_every_;
    bool verbose_;
    std::atomic_bool stop_{false};
    std::atomic_bool eos_{false};
    std::atomic_bool abnormal_{false};
    std::atomic<uint64_t> frames_{0};
    std::atomic<uint64_t> bytes_{0};
    std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
    std::mutex wait_mutex_;
    std::condition_variable wait_condition_;
};

inline RunMonitor*& signalMonitor()
{
    static RunMonitor* monitor = nullptr;
    return monitor;
}

inline void stopSignalHandler(int)
{
    if (signalMonitor())
        signalMonitor()->requestStopFromSignal();
}

inline void installSignalHandlers(RunMonitor& monitor)
{
    signalMonitor() = &monitor;
    std::signal(SIGINT, stopSignalHandler);
    std::signal(SIGTERM, stopSignalHandler);
}

inline void dumpExtraBuffer(const char* name,
                            const std::shared_ptr<FFMedia::MediaBuffer>& buffer)
{
    ff_info("%s extra data: %zu bytes\n", name,
            buffer ? buffer->getActiveSize() : 0);
}

}  // namespace FFMediaTest
