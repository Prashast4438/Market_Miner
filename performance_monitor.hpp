#pragma once

#include <chrono>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <spdlog/spdlog.h>
#include <algorithm>

class PerformanceMonitor {
public:
    static PerformanceMonitor& instance() {
        static PerformanceMonitor instance;
        return instance;
    }

    void start_operation(const std::string& operation_name) {
        auto& metrics = get_metrics(operation_name);
        metrics.start_time = std::chrono::high_resolution_clock::now();
    }

    void end_operation(const std::string& operation_name) {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto& metrics = get_metrics(operation_name);
        
        if (metrics.start_time.time_since_epoch().count() == 0) {
            spdlog::warn("Operation {} ended without start", operation_name);
            return;
        }

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - metrics.start_time).count() / 1000.0;  // Convert to milliseconds

        metrics.durations.push_back(duration);
        metrics.total_duration += duration;
        metrics.count++;

        // Log if latency is above threshold
        if (duration > 100.0) {  // 100ms threshold
            spdlog::warn("High latency detected for {}: {:.2f}ms", operation_name, duration);
        }

        // Keep only last 1000 measurements
        if (metrics.durations.size() > 1000) {
            metrics.total_duration -= metrics.durations.front();
            metrics.durations.erase(metrics.durations.begin());
        }
    }

    double get_average_latency(const std::string& operation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = operation_metrics_.find(operation_name);
        if (it == operation_metrics_.end() || it->second.count == 0) {
            return 0.0;
        }
        return it->second.total_duration / it->second.count;
    }

    void print_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::info("Performance Metrics:");
        for (const auto& pair : operation_metrics_) {
            if (pair.second.count > 0) {
                spdlog::info("{}: Avg Latency = {:.2f}ms, Count = {}",
                    pair.first,
                    pair.second.total_duration / pair.second.count,
                    pair.second.count);
            }
        }
    }

    void track_websocket_latency(const std::string& symbol, double latency_ms) {
        std::string operation_name = "websocket_" + symbol;
        auto& metrics = get_metrics(operation_name);
        metrics.durations.push_back(latency_ms);
        metrics.total_duration += latency_ms;
        metrics.count++;
        
        if (latency_ms > 50.0) {  // 50ms threshold for WebSocket
            spdlog::warn("High WebSocket latency for {}: {:.2f}ms", symbol, latency_ms);
        }
    }

    double get_percentile_latency(const std::string& operation_name, double percentile) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = operation_metrics_.find(operation_name);
        if (it == operation_metrics_.end() || it->second.durations.empty()) {
            return 0.0;
        }

        std::vector<double> sorted_durations = it->second.durations;
        std::sort(sorted_durations.begin(), sorted_durations.end());
        size_t index = static_cast<size_t>(percentile * sorted_durations.size());
        return sorted_durations[index];
    }

    void print_detailed_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        spdlog::info("Detailed Performance Metrics:");
        for (const auto& pair : operation_metrics_) {
            if (pair.second.count > 0) {
                double p50 = get_percentile_latency(pair.first, 0.50);
                double p95 = get_percentile_latency(pair.first, 0.95);
                double p99 = get_percentile_latency(pair.first, 0.99);
                
                spdlog::info("{}: Avg={:.2f}ms, p50={:.2f}ms, p95={:.2f}ms, p99={:.2f}ms, Count={}",
                    pair.first,
                    pair.second.total_duration / pair.second.count,
                    p50, p95, p99,
                    pair.second.count);
            }
        }
    }

private:
    struct OperationMetrics {
        std::chrono::high_resolution_clock::time_point start_time;
        std::vector<double> durations;
        double total_duration = 0.0;
        size_t count = 0;
    };

    OperationMetrics& get_metrics(const std::string& operation_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return operation_metrics_[operation_name];
    }

    std::map<std::string, OperationMetrics> operation_metrics_;
    std::mutex mutex_;
};
