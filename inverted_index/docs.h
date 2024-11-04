#pragma once

#include <assert.h>
#include <string>
#include <bitset>
#include <sstream>
#include <vector>

template <std::size_t MaxDocCount>
class TDocs {
public:
    TDocs() = default;
    TDocs(std::bitset<MaxDocCount> docs)
            : Docs(std::move(docs))
    {}

    void Add(std::size_t ID) {
        assert(ID < MaxDocCount);
        Docs[ID] = 1;
    }

    bool HasDoc(std::size_t ID) const {
        return ID < MaxDocCount && Docs[ID];
    }

    std::vector<size_t> GetIDs() const {
        std::vector<std::size_t> docs;

        for (std::size_t i = 0; i < MaxDocCount; ++i) {
            if (Docs[i]) {
                docs.push_back(i);
            }
        }

        return docs;
    }

    bool operator==(const TDocs<MaxDocCount>& other) const {
        return this->Docs == other.Docs;
    }

    bool operator<(const TDocs<MaxDocCount>& other) const {
        return false;
    }

    TDocs<MaxDocCount>& And(const TDocs<MaxDocCount>& other) {
        Docs &= other.Docs;
        return *this;
    }

    TDocs<MaxDocCount>& Or(const TDocs<MaxDocCount>& other) {
        Docs |= other.Docs;
        return *this;
    }

    TDocs<MaxDocCount>& Not() {
        Docs = ~Docs;
        return *this;
    }

    std::string String() {
        std::stringstream s;
        for (const auto& i: GetIDs()) {
            s << i << ",";
        }
        return s.str();
    }

    void SetAll() {
        Docs.set();
    }

private:
    std::bitset<MaxDocCount> Docs;
};
