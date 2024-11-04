#pragma once

#include <string>
#include <vector>

namespace NUtils {

    std::vector<std::string> Split(const std::string &str, char delimiter) {
        std::vector<std::string> result;
        std::string token;
        for (char ch: str) {
            if (ch != delimiter) {
                token += ch;
            } else {
                if (!token.empty()) {
                    result.push_back(token);
                    token.clear();
                }
            }
        }
        if (!token.empty()) {
            result.push_back(token);
        }
        return result;
    }

}