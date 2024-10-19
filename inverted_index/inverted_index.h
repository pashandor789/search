#pragma once

#include <cstdio>
#include <string>
#include "../lsm/lsm.h"
#include "../lsm/types.h"
#include "text_processor.h"

struct TDocument {
    std::size_t ID;
    std::string Text;
};

template <std::size_t MaxDocCount>
class TDocs {
public:
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

private:
    std::bitset<MaxDocCount> Docs;
};

template <std::size_t MaxDocCount>
class TInvertedIndex {
public:
    TInvertedIndex(std::filesystem::path indexStoragePath)
        : LSMTree(std::move(indexStoragePath))
    {}

    void AddDocument(const TDocument& doc) {
        assert(doc.ID < MaxDocCount);

        for (const auto& processedWord: Processor.Process(doc.Text)) {
            if (auto maybeEntry = LSMTree.ReadPoint(processedWord)) {
                auto& [word, docs] = maybeEntry.value();
                docs.Add(doc.ID);
                LSMTree.Insert(word, docs);
                continue;
            }

            TDocs<MaxDocCount> docs;
            docs.Add(doc.ID);
            LSMTree.Insert(processedWord, docs);
        }
    }

    std::vector<size_t> FindDocsByWord(const std::string& word) {
        std::vector<std::size_t> res;

        auto searchingWord = Processor.Process(word)[0];
        if (auto maybeEntry = LSMTree.ReadPoint(searchingWord)) {
            const auto& [_, docs] = maybeEntry.value();
            return docs.GetIDs();
        }

        return res;
    }

private:
    using TWord = TString<128>;
    TLSMTree<TWord, TDocs<MaxDocCount>> LSMTree;
    TTextProcessor Processor;
};