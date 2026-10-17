// SPDX-License-Identifier: GPL-3.0-or-later
#include "mozc_backend.h"
#include "base/init_mozc.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iostream>

int main(int argc, char** argv) {
    mozc::InitMozc(argv[0], &argc, &argv);
    tailkana::MozcBackend backend;
    std::string roman;
    while (std::getline(std::cin, roman)) {
        nlohmann::json result = {{"reading", ""}, {"candidates", nlohmann::json::array()}};
        if (!roman.empty() && roman.size() <= 96 &&
            roman.find_first_not_of("abcdefghijklmnopqrstuvwxyz'-") == std::string::npos && backend.query(roman, true, false, false)) {
            result["reading"] = backend.composingReading;
            for (const auto& candidate : backend.candidates) {
                // Score complete compositions, including unchanged trailing segments.
                if (result["candidates"].size() == 12) break;
                const int index = &candidate - backend.candidates.data();
                if (!backend.highlight(index)) continue;
                const auto& full = backend.reading;
                if (!full.empty() && std::none_of(result["candidates"].begin(), result["candidates"].end(),
                    [&](const auto& item) { return item["text"] == full; }))
                    result["candidates"].push_back({{"text", full}, {"reading", candidate.reading}});
            }
        }
        // Never SUBMIT or SELECT_CANDIDATE: a trial must not train the real profile.
        backend.clear();
        std::cout << result.dump() << std::endl;
    }
}
