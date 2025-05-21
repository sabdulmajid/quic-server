#ifndef FEATURES_H
#define FEATURES_H

#include <vector>
#include <fstream>
#include <chrono>
#include "teleop_generated.h"

class LatencyStats {
    std::chrono::milliseconds total{0};
    size_t count{0};
public:
    void add(uint64_t sentTs) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > sentTs) {
            total += std::chrono::milliseconds(now - sentTs);
            ++count;
        }
    }
    double average() const {
        return count ? static_cast<double>(total.count()) / static_cast<double>(count) : 0.0;
    }
};

class CommandLogger {
    std::ofstream file;
public:
    bool open(const std::string& path) {
        file.open(path, std::ios::app);
        return file.is_open();
    }
    void log(const Teleop::ControlCommandT& cmd) {
        if (!file.is_open()) return;
        file << cmd.timestamp << "," << cmd.linear_velocity << "," << cmd.angular_velocity << "\n";
    }
};

class MacroRecorder {
    std::vector<Teleop::ControlCommandT> buffer;
    bool recording{false};
public:
    void start() { recording = true; buffer.clear(); }
    void stop() { recording = false; }
    void record(const Teleop::ControlCommandT& cmd) { if (recording) buffer.push_back(cmd); }
    const std::vector<Teleop::ControlCommandT>& get() const { return buffer; }
    bool isRecording() const { return recording; }
};

#endif // FEATURES_H
