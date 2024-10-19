#pragma once

#include <array>
#include <algorithm>
#include <string>

// fixed size string
template <size_t Size>
class TString {
private:
    std::array<char, Size> arr;

public:
    TString(std::string str) {
        if (str.size() > arr.size()) {
            throw std::out_of_range("String size exceeds capacity of TString.");
        }
        std::copy(str.begin(), str.end(), arr.begin());
        if (str.size() < arr.size()) {
            std::fill(arr.begin() + str.size(), arr.end(), '\0');
        }
    }

    TString(const char* str)
            : TString(std::string(str))
    {}

    TString() = default;

    std::size_t hash() const {
        return std::hash<std::string>()(std::string(arr.data()));
    }

    bool operator<(const TString& other) const {
        return this->arr < other.arr;
    }

    bool operator<=(const TString& other) const {
        return this->arr <= other.arr;
    }

    bool operator>(const TString& other) const {
        return this->arr > other.arr;
    }

    bool operator==(const TString& other) const {
        return this->arr == other.arr;
    }
};
