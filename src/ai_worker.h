// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace tailkana {
struct AiRequest {
    std::array<uint8_t,16> uuid{};
    uint64_t revision=0;
    std::string raw,context;
    int maxLength=32;
    bool katakana=false;
};
struct AiResult {
    AiRequest request;
    std::string text,reading,status;
    int elapsedMs=0;
};
// Only the worker thread touches subprocesses. All callbacks must be dispatched
// back to the Fcitx loop before accessing any InputContext or Mozc session.
class AiWorker {
public:
    using Callback=std::function<void(AiResult)>;
    explicit AiWorker(Callback);
    ~AiWorker();
    void enable(bool);
    void submit(AiRequest);
    void cancel(const std::array<uint8_t,16>&);
    std::string status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string utf8Tail(const std::string&,size_t characters);
std::string contextBefore(const std::string&,size_t cursor,size_t characters);
}
