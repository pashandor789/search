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
    struct TOpts {
        TOpts() {}
        TOpts(bool addNGrams, bool addStemming, bool removeStopWords)
            : AddNGrams(addNGrams)
            , AddStemming(addStemming)
            , RemoveStopWords(removeStopWords)
        {}

        bool AddNGrams = false;
        bool AddStemming = true;
        bool RemoveStopWords = true;
    };

    std::vector<std::string> Process(std::string text, TOpts opts = {}) {
        std::transform(text.begin(), text.end(), text.begin(), ::tolower);
        text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
            return !std::isalnum(c) && !std::isspace(c);
        }), text.end());
        std::vector<std::string> tokens = Tokenize(text);
        if (opts.RemoveStopWords) {
            tokens.erase(std::remove_if(tokens.begin(), tokens.end(), [this](const std::string &token) {
                return std::find(STOP_WORDS.begin(), STOP_WORDS.end(), token) != STOP_WORDS.end();
            }), tokens.end());
        }

        std::vector<std::string> processedWords;
        if (opts.AddStemming) {
            for (std::string &token: tokens) {
                processedWords.push_back(token);
            }
        } else {
            processedWords = tokens;
        }

        if (opts.AddNGrams) {
            std::vector<std::string> nGrams;
            for (const auto& token: tokens) {
                for (size_t k = 1; k <= token.size(); ++k) {
                    auto kGram = GenerateKGram(token, k);
                    nGrams.insert(nGrams.end(), std::make_move_iterator(kGram.begin()), std::make_move_iterator(kGram.end()));
                }
            }
            processedWords = std::move(nGrams);
        }

        return processedWords;
    }

private:
    TStemmer stemmer;
    std::vector<std::string> STOP_WORDS = {"the", "and", "is", "in", "at", "of", "a", "on"};

    static std::vector<std::string> GenerateKGram(const std::string& token, size_t k) {
        std::vector<std::string> nGrams;
        nGrams.reserve(token.size() - k + 1);

        for (size_t i = 0; i < token.size() - k + 1; ++i) {
            nGrams.push_back(token.substr(0, k));
        }

        return nGrams;
    }

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
