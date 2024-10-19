#pragma once

#include "../contrib/OleanderStemmingLibrary/src/english_stem.h"

#include <algorithm>
#include <vector>
#include <string>
#include <ostream>
#include <istream>
#include <sstream>


class TStemmer {
public:
    void Stem(std::string& text) {
        std::wstring s(text.begin(), text.end());
        stemming::english_stem<> stemmer;
        stemmer(s);
        text = std::string(s.begin(), s.end());
    }
};

class TTextProcessor {
public:
    std::vector<std::string> Process(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
            return !std::isalnum(c) && !std::isspace(c);
        }), text.end());
        std::vector<std::string> tokens = Tokenize(text);
        std::vector<std::string> processedWords;
        for (std::string& token : tokens) {
            if (std::find(STOP_WORDS.begin(), STOP_WORDS.end(), token) == STOP_WORDS.end()) {
                stemmer.Stem(token);
                processedWords.push_back(token);
            }
        }
        return processedWords;
    }

private:
    TStemmer stemmer;
    std::vector<std::string> STOP_WORDS = {"the", "and", "is", "in", "at", "of", "a", "on"};

    static std::vector<std::string> Tokenize(const std::string& text) {
        std::vector<std::string> tokens;
        std::istringstream stream(text);
        std::string token;
        while (stream >> token) {
            tokens.push_back(token);
        }
        return tokens;
    }
};
