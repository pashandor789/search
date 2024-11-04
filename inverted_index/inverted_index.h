#pragma once

#include <type_traits>
#include <cstdio>
#include <string>
#include "../lsm/lsm.h"
#include "../lsm/types.h"
#include "text_processor.h"
#include "utils.h"
#include "logic_algebra.h"
#include "docs.h"

struct TDocument {
    std::size_t ID;
    std::string Text;
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

    TDocs<MaxDocCount> FindDocsByWord(const std::string& word) {
        auto searchingWord = Processor.Process(word)[0];
        if (auto maybeEntry = LSMTree.ReadPoint(searchingWord)) {
            const auto& [_, docs] = maybeEntry.value();
            return docs;
        }

        return TDocs<MaxDocCount>();
    }

    TDocs<MaxDocCount> FindDocsByExpr(const std::shared_ptr<NLogicAlgebra::IASTNode>& astTree) {
        auto ctx = NLogicAlgebra::IASTNode::TContext([this](const std::string& word){ return FindDocsByWord(word); });
        return astTree->Evaluate(ctx);
    }

private:
    using TWord = TString<128>;
    TLSMTree<TWord, TDocs<MaxDocCount>> LSMTree;
    TTextProcessor Processor;
};

template <std::size_t MaxDocCount>
class TInvertedPatternIndex {
public:
    TInvertedPatternIndex(std::filesystem::path indexStoragePath)
        : LSMTree(std::move(indexStoragePath))
    {}

    void AddDocument(const TDocument& doc) {
        assert(doc.ID < MaxDocCount);

        for (const auto& processedWord: Processor.Process(doc.Text, TTextProcessor::TOpts(true, false, true))) {
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

        docsStorage.push_back(doc);
    }

    TDocs<MaxDocCount> FindDocsByPattern(const std::string& pattern) {
        std::shared_ptr<NLogicAlgebra::IASTNode> dummy;

        for (const auto& toSearch: NUtils::Split(pattern, '*')) {
            dummy = NLogicAlgebra::And(dummy, toSearch);
        }

        auto ctx = NLogicAlgebra::IASTNode::TContext([this](const std::string& word){ return FindDocsByWord(word); });
        return MatchPattern(dummy->Evaluate(ctx), pattern);
    }

    TDocs<MaxDocCount> FindDocsByPrefix(const std::string& prefix) {
        return FindDocsByPattern(prefix + "*");
    }

private:
    TDocs<MaxDocCount> MatchPattern(const TDocs<MaxDocCount>& docs, const std::string& pattern) {
        TDocs<MaxDocCount> res;
        if (pattern.empty()) {
            return res;
        }

        auto order = NUtils::Split(pattern, '*');
        for (const auto& docID: docs.GetIDs()) {
            auto& doc = docsStorage[docID];

            size_t prevPos = 0;
            for (size_t idx = 0; idx < order.size(); ++idx) {
                if (idx > 0 && order[idx - 1] == order[idx]) {
                    ++prevPos;
                }
                size_t curPos = doc.Text.find(order[idx], prevPos);
                if ((prevPos = curPos) == std::string::npos) {
                    break;
                }
            }

            if (prevPos == std::string::npos) {
                continue;
            }

            if (pattern.front() != '*') {
                auto foundID = doc.Text.find(order.front());
                if (foundID != 0 && doc.Text[foundID - 1] != ' ') {
                    continue;
                }
            }

            if (pattern.back() != '*') {
                auto foundID = doc.Text.find(order.back());

                if (foundID + order.back().size() < doc.Text.size() && doc.Text[foundID + order.back().size()] != ' ') {
                    continue;
                }
            }

            res.Add(docID);
        }

        return res;
    }

private:
    TDocs<MaxDocCount> FindDocsByWord(const std::string& word) {
        auto processedWord = Processor.Process(word, TTextProcessor::TOpts(false, false, false));
        std::string searchingWord = processedWord[0];
        if (auto maybeEntry = LSMTree.ReadPoint(searchingWord)) {
            const auto& [_, docs] = maybeEntry.value();
            return docs;
        }

        return TDocs<MaxDocCount>();
    }

private:
    using TWord = TString<128>;
    TLSMTree<TWord, TDocs<MaxDocCount>> LSMTree;
    std::vector<TDocument> docsStorage;
    TTextProcessor Processor;
};
