#pragma once

// Trace support for the ffmedia demo. Included after the demo graph types.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/media_metrics.hpp"

using MediaMetrics = ::FFMedia::MediaMetrics;
int64_t traceNowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int64_t traceWallNowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int64_t traceLocalLatencyUs(int64_t pts_us)
{
    // Zero/negative PTS is the MediaBuffer default, not a clock timestamp.
    if (pts_us <= 0)
        return -1;
    // A relative stream PTS can look like a monotonic timestamp after startup.
    // Keep the accepted local-clock age bounded so it cannot become uptime.
    constexpr int64_t kMaxLocalLatencyUs = 10LL * 60 * 1000000;
    const int64_t wall_latency = traceWallNowUs() - pts_us;
    if (wall_latency >= 0 && wall_latency <= kMaxLocalLatencyUs)
        return wall_latency;

    const int64_t monotonic_latency = traceNowUs() - pts_us;
    if (monotonic_latency >= 0 && monotonic_latency <= kMaxLocalLatencyUs)
        return monotonic_latency;
    return -1;
}

struct TraceDistribution {
    uint64_t samples = 0;
    uint64_t invalid_samples = 0;
    int64_t min_us = std::numeric_limits<int64_t>::max();
    int64_t max_us = 0;
    long double sum_us = 0;
    long double sum_sq_us = 0;
    std::vector<int64_t> values;

    void add(int64_t value)
    {
        if (value < 0) {
            ++invalid_samples;
            return;
        }
        ++samples;
        min_us = std::min(min_us, value);
        max_us = std::max(max_us, value);
        sum_us += value;
        sum_sq_us += static_cast<long double>(value) * value;
        constexpr size_t kMaxValues = 32768;
        if (values.size() < kMaxValues) {
            values.push_back(value);
            return;
        }

        // Replace a bounded sample so percentiles remain representative.
        reservoir_state = reservoir_state * 6364136223846793005ULL + 1;
        const uint64_t slot = reservoir_state % samples;
        if (slot < kMaxValues)
            values[static_cast<size_t>(slot)] = value;
    }

    uint64_t reservoir_state = 0x9e3779b97f4a7c15ULL;
};

struct TraceModuleStats {
    uint64_t consumed_seen = 0;
    uint64_t produced_seen = 0;
};

struct TraceSourceSample {
    int64_t produce_us = 0;
    int64_t pts_us = 0;
    MEDIA_BUFFER_TYPE media_type = BUFFER_TYPE_ETC;
};

struct TracePointPair {
    size_t source_index = 0;
    size_t target_index = 0;
    std::string source_name;
    std::string target_name;
    std::deque<TraceSourceSample> sources;
    TraceDistribution latency;
    uint64_t unmatched = 0;
    uint64_t source_overflow = 0;
};

struct TracePointReport {
    std::string source_name;
    std::string target_name;
    TraceDistribution latency;
    uint64_t unmatched = 0;
    uint64_t source_overflow = 0;
};

class PipelineTrace
{
public:
    static constexpr size_t kMaxPendingSourceFrames = 8192;
    static constexpr uint64_t kWarmupFrames = 1;

    PipelineTrace(const std::vector<ModuleInstance>& instances,
                  const GraphPlan& plan, uint64_t interval_ms,
                  const std::vector<std::pair<std::string, std::string>>& point_pairs,
                  const std::vector<std::string>& local_latency_modules,
                  const std::vector<std::string>& trace_modules,
                  bool collect_modules, bool collect_latency,
                  bool collect_local_latency)
        : interval_ms_(interval_ms), collect_modules_(collect_modules),
          collect_latency_(collect_latency),
          collect_local_latency_(collect_local_latency)
    {
        start_us_ = traceNowUs();
        last_report_us_ = start_us_;
        module_names_.reserve(instances.size());
        module_media_.reserve(instances.size());
        modules_.resize(instances.size());
        point_pair_sources_by_module_.resize(instances.size());
        point_pair_targets_by_module_.resize(instances.size());
        local_module_stats_.resize(instances.size());
        root_flags_.assign(instances.size(), false);
        local_latency_flags_.assign(instances.size(), false);
        for (size_t i = 0; i < instances.size(); ++i) {
            module_names_.push_back(instances[i].id);
            module_media_.push_back(instances[i].module);
            module_index_.emplace(instances[i].id, i);
        }
        for (size_t root : plan.roots) {
            root_flags_[root] = true;
        }
        for (const auto& point_pair : point_pairs) {
            TracePointPair trace_pair;
            trace_pair.source_index = module_index_.at(point_pair.first);
            trace_pair.target_index = module_index_.at(point_pair.second);
            trace_pair.source_name = point_pair.first;
            trace_pair.target_name = point_pair.second;
            const size_t pair_index = point_pairs_.size();
            point_pairs_.push_back(std::move(trace_pair));
            point_pair_sources_by_module_[point_pairs_.back().source_index]
                .push_back(pair_index);
            point_pair_targets_by_module_[point_pairs_.back().target_index]
                .push_back(pair_index);
        }
        for (const auto& module : local_latency_modules) {
            const size_t index = module_index_.at(module);
            local_latency_targets_.insert(index);
            local_latency_flags_[index] = true;
        }
        for (const auto& module : trace_modules)
            trace_module_targets_.insert(module_index_.at(module));
    }

    size_t moduleIndex(const std::string& module) const
    {
        return module_index_.at(module);
    }

    uint64_t intervalMs() const
    {
        return interval_ms_;
    }

private:
    bool readMetric(size_t module_index, MediaMetrics& metrics) const
    {
        metrics = MediaMetrics{};
        const auto& module = module_media_[module_index];
        return module && module->getMetrics(metrics) == 0;
    }

    static bool takeMatchingSource(
        std::deque<TraceSourceSample>& sources,
        const std::shared_ptr<MediaBuffer>& buffer,
        TraceSourceSample& matched, uint64_t& unmatched,
        bool allow_fifo_without_pts = false)
    {
        const MEDIA_BUFFER_TYPE type = buffer->getMediaBufferType();
        const int64_t pts_us = buffer->getPUstimestamp();
        for (auto source = sources.begin(); source != sources.end();) {
            if (source->media_type != type) {
                ++source;
                continue;
            }

            if (pts_us > 0 && source->pts_us == pts_us) {
                matched = *source;
                sources.erase(source);
                return true;
            }
            if (pts_us > 0 && source->pts_us <= 0) {
                source = sources.erase(source);
                ++unmatched;
                continue;
            }
            if (pts_us > 0 && source->pts_us < pts_us) {
                source = sources.erase(source);
                ++unmatched;
                continue;
            }
            if (pts_us <= 0 && allow_fifo_without_pts) {
                matched = *source;
                sources.erase(source);
                return true;
            }
            ++source;
        }
        return false;
    }

public:
    void tick()
    {
        const int64_t now = traceNowUs();
        if (now - last_report_us_
            >= static_cast<int64_t>(interval_ms_) * 1000)
            report(false);
    }

    void reportFinal()
    {
        report(true);
    }

    void onConsume(size_t module_index, int queue_size,
                   const std::shared_ptr<MediaBuffer>& buffer)
    {
        if (!buffer)
            return;
        const int64_t now = traceNowUs();
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = modules_[module_index];
        ++stats.consumed_seen;
        if (!collect_modules_ && !collect_latency_ && !collect_local_latency_)
            return;
        if (stats.consumed_seen <= kWarmupFrames)
            return;
        if (collect_local_latency_
            && local_latency_flags_[module_index]
            && !root_flags_[module_index]) {
            const int64_t local_latency = traceLocalLatencyUs(
                buffer->getPUstimestamp());
            if (local_latency >= 0)
                local_module_stats_[module_index].add(local_latency);
            else
                ++invalid_timestamp_samples_;
        }
        for (const size_t pair_index :
             point_pair_targets_by_module_[module_index]) {
            auto& point_pair = point_pairs_[pair_index];
            TraceSourceSample source;
            if (!takeMatchingSource(point_pair.sources, buffer, source,
                                    point_pair.unmatched)) {
                ++point_pair.unmatched;
                continue;
            }
            point_pair.latency.add(now - source.produce_us);
        }
    }

    void onProduce(size_t module_index, int,
                   const std::shared_ptr<MediaBuffer>& buffer)
    {
        if (!buffer)
            return;
        const int64_t now = traceNowUs();
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = modules_[module_index];
        ++stats.produced_seen;
        if (!collect_modules_ && !collect_latency_ && !collect_local_latency_)
            return;
        if (stats.produced_seen <= kWarmupFrames)
            return;
        const int64_t pts_us = buffer->getPUstimestamp();
        if (collect_local_latency_
            && local_latency_flags_[module_index]
            && root_flags_[module_index]
            && stats.produced_seen > kWarmupFrames) {
            const int64_t local_latency = traceLocalLatencyUs(pts_us);
            if (local_latency >= 0)
                local_module_stats_[module_index].add(local_latency);
            else
                ++invalid_timestamp_samples_;
        }
        if (stats.produced_seen > kWarmupFrames) {
            for (const size_t pair_index :
                 point_pair_sources_by_module_[module_index]) {
                auto& sources = point_pairs_[pair_index].sources;
                sources.push_back(TraceSourceSample{
                    now, pts_us, buffer->getMediaBufferType()});
                if (sources.size() > kMaxPendingSourceFrames) {
                    sources.pop_front();
                    ++point_pairs_[pair_index].source_overflow;
                }
            }
        }
    }

private:
    static double fps(uint64_t count, int64_t elapsed_us)
    {
        return elapsed_us > 0 ? count * 1000000.0 / elapsed_us : 0.0;
    }

    static uint64_t delta(uint64_t current, uint64_t previous)
    {
        return current >= previous ? current - previous : 0;
    }

    void report(bool final)
    {
        std::vector<MediaMetrics> metrics;
        uint64_t invalid_timestamps = 0;
        std::vector<MediaMetrics> previous_metrics;
        std::vector<TracePointReport> point_pairs;
        std::vector<TraceDistribution> local_module_stats;
        const int64_t report_now_us = traceNowUs();
        int64_t interval_elapsed_us = 1;
        uint64_t snapshot_failures = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            metrics.resize(module_media_.size());
            for (size_t i = 0; i < module_media_.size(); ++i) {
                if (!readMetric(i, metrics[i]))
                    ++snapshot_failures;
            }
            invalid_timestamps = invalid_timestamp_samples_;
            point_pairs.reserve(point_pairs_.size());
            for (const auto& point_pair : point_pairs_)
                point_pairs.push_back(TracePointReport{
                    point_pair.source_name, point_pair.target_name,
                    point_pair.latency, point_pair.unmatched,
                    point_pair.source_overflow});
            local_module_stats = local_module_stats_;
            interval_elapsed_us = std::max<int64_t>(
                1, report_now_us - last_report_us_);
            previous_metrics = last_report_metrics_;
            last_report_us_ = report_now_us;
            last_report_metrics_ = metrics;
            metrics_snapshot_failures_ += snapshot_failures;
        }
        const int64_t elapsed_us = std::max<int64_t>(1, report_now_us - start_us_);
        std::cout << "[trace] " << (final ? "final" : "interval")
                  << " elapsed=" << std::fixed << std::setprecision(3)
                  << elapsed_us / 1000000.0 << "s"
                  << " counters=total rates=interval";
        if (collect_modules_)
            std::cout << " metrics=ModuleMedia::getMetrics";
        if (collect_latency_)
            std::cout << " correlation=pts-matched"
                      << " endpoint=consume-arrival"
                      << " warmup-skip=" << kWarmupFrames;
        std::cout << "\n";
        if (collect_modules_) {
            for (size_t i = 0; i < metrics.size(); ++i) {
                if (!trace_module_targets_.empty()
                    && trace_module_targets_.count(i) == 0)
                    continue;
                const MediaMetrics& value = metrics[i];
                const MediaMetrics previous_value = i < previous_metrics.size()
                                                        ? previous_metrics[i]
                                                        : MediaMetrics{};
                const uint64_t interval_in = delta(
                    value.input_frames, previous_value.input_frames);
                const uint64_t interval_out = delta(
                    value.output_frames, previous_value.output_frames);
                std::cout << "[trace] module=" << module_names_[i]
                          << " in=" << value.input_frames
                          << " out=" << value.output_frames
                          << " in-fps=" << std::setprecision(2)
                          << fps(interval_in, interval_elapsed_us)
                          << " out-fps=" << fps(interval_out, interval_elapsed_us)
                          << " metrics-enabled=" << (value.enabled ? 1 : 0)
                          << " queue-last=" << value.queue_depth
                          << " queue-max-total=" << value.queue_peak_depth
                          << " process-samples=" << value.processing_samples
                          << " process-ms[last/avg/max]="
                          << value.processing_time_new_usec / 1000.0 << "/"
                          << value.processing_time_avg_usec / 1000.0 << "/"
                          << value.processing_time_max_usec / 1000.0
                          << " dropped-in=" << value.input_dropped_frames
                          << " dropped-process="
                          << value.processing_dropped_frames
                          << " dropped-out=" << value.output_dropped_frames
                          << " buffer-pool-exhausted="
                          << value.buffer_pool_exhausted
                          << " errors=" << value.error_count
                          << "\n";
            }
            if (metrics_snapshot_failures_)
                std::cout << "[trace] metrics-snapshot-failures-total="
                          << metrics_snapshot_failures_ << "\n";
        }
        if ((collect_latency_ || collect_local_latency_)
            && invalid_timestamps)
            std::cout << "[trace] invalid-local-timestamps="
                      << invalid_timestamps << "\n";
        for (const auto& point_pair : point_pairs) {
            const std::string name = "point-to-point " + point_pair.source_name
                                     + "->" + point_pair.target_name;
            if (point_pair.latency.samples)
                printDistribution(name.c_str(), point_pair.latency);
            else
                std::cout << "[trace] " << name << " samples=0\n";
            if (point_pair.unmatched)
                std::cout << "[trace] " << name
                          << " unmatched=" << point_pair.unmatched << "\n";
            if (point_pair.source_overflow)
                std::cout << "[trace] " << name
                          << " source-overflow="
                          << point_pair.source_overflow << "\n";
        }
        for (const size_t module : local_latency_targets_) {
            const std::string name = "local-latency module=" + module_names_[module];
            if (module < local_module_stats.size()
                && local_module_stats[module].samples)
                printDistribution(name.c_str(), local_module_stats[module]);
            else
                std::cout << "[trace] " << name << " samples=0\n";
        }
    }

    static void printDistribution(const char* name, TraceDistribution stats)
    {
        std::sort(stats.values.begin(), stats.values.end());
        const auto percentile = [&stats](double p) {
            if (stats.values.empty())
                return 0.0;
            const size_t index = static_cast<size_t>(
                p * (stats.values.size() - 1));
            return stats.values[index] / 1000.0;
        };
        const double mean = static_cast<double>(stats.sum_us / stats.samples);
        const double variance = std::max<double>(
            0.0, static_cast<double>(stats.sum_sq_us / stats.samples)
                     - mean * mean);
        std::cout << "[trace] " << name << " samples=" << stats.samples
                  << " latency-ms[min/avg/p50/p95/p99/max]="
                  << stats.min_us / 1000.0 << "/" << mean / 1000.0 << "/"
                  << percentile(0.50) << "/" << percentile(0.95) << "/"
                  << percentile(0.99) << "/" << stats.max_us / 1000.0
                  << " stddev-ms=" << std::sqrt(variance) / 1000.0
                  << " invalid=" << stats.invalid_samples << "\n";
    }

    uint64_t interval_ms_;
    bool collect_modules_;
    bool collect_latency_;
    bool collect_local_latency_;
    int64_t start_us_ = 0;
    std::vector<std::string> module_names_;
    std::unordered_map<std::string, size_t> module_index_;
    std::vector<std::shared_ptr<ModuleMedia>> module_media_;
    std::vector<TraceModuleStats> modules_;
    std::vector<bool> root_flags_;
    std::vector<bool> local_latency_flags_;
    uint64_t invalid_timestamp_samples_ = 0;
    int64_t last_report_us_ = 0;
    std::vector<MediaMetrics> last_report_metrics_;
    uint64_t metrics_snapshot_failures_ = 0;
    std::vector<TracePointPair> point_pairs_;
    std::vector<std::vector<size_t>> point_pair_sources_by_module_;
    std::vector<std::vector<size_t>> point_pair_targets_by_module_;
    std::set<size_t> local_latency_targets_;
    std::set<size_t> trace_module_targets_;
    std::vector<TraceDistribution> local_module_stats_;
    std::mutex mutex_;
};
