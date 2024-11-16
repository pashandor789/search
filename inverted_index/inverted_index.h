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

namespace NBitSliceIndex {
    class TRangePredicate {
    public:
        TRangePredicate(std::size_t maxBitCount = 32)
            : MaxBitCount(maxBitCount)
        {}

        std::vector<std::vector<bool>> GetPredicates(uint32_t l, uint32_t r) {
            GetPredicatesImpl(0, std::numeric_limits<std::uint32_t>::max(), l, r);
            return Predicates;
        }

    private:
        void GetPredicatesImpl(
            uint32_t curLeft,
            uint32_t curRight,
            uint32_t requestedLeft,
            uint32_t requestedRight
        ) {
            if (curLeft > curRight || requestedLeft > requestedRight) {
                return;
            }

            if (curLeft == requestedLeft && curRight == requestedRight) {
                Predicates.push_back(Path);
                return;
            }

            uint32_t mid = (curLeft + curRight) / 2;

            Path.push_back(false);
            GetPredicatesImpl(curLeft, mid, requestedLeft, std::min(requestedRight, mid));
            Path.pop_back();

            Path.push_back(true);
            GetPredicatesImpl(mid + 1, curRight, std::max(requestedLeft, mid + 1), requestedRight);
            Path.pop_back();
        }

    private:
        std::vector<bool> Path;
        std::vector<std::vector<bool>> Predicates;
        const std::size_t MaxBitCount = 0;
    };
}

class TInvertedDateIntervalIndex {
public:
    TInvertedDateIntervalIndex() {
        DocIDsByBitStart.resize(32);
        DocIDsByBitEnd.resize(32);
    }

    void AddDocument(const TDocument& doc, uint32_t intervalBegin, uint32_t intervalEnd) {
        AddedDocs.Add(doc.ID);

        for (std::size_t i = 0; i < 32; ++i) {
            if ((intervalBegin >> (32 - 1 - i)) & 1) {
                DocIDsByBitStart[i].Add(doc.ID);
            }

            if ((intervalEnd >> (32 - 1 - i)) & 1) {
                DocIDsByBitEnd[i].Add(doc.ID);
            }
        }
    }

    TDocs<128> FindDocsByInterval(uint32_t intervalBegin, uint32_t intervalEnd) {
        auto predicates1 = NBitSliceIndex::TRangePredicate().GetPredicates(0, intervalEnd);
        auto intervalSuitableDocsStarts = EvaluatePredicates(predicates1, DocIDsByBitStart);
        auto predicates2 = NBitSliceIndex::TRangePredicate().GetPredicates(intervalBegin, std::numeric_limits<uint32_t>::max());
        auto intervalSuitableDocsEnds = EvaluatePredicates(predicates2, DocIDsByBitEnd);
        return intervalSuitableDocsStarts.And(intervalSuitableDocsEnds);
    }

    TDocs<128> FindDocsByTimePoint(uint32_t timestamp) {
        return FindDocsByInterval(timestamp, timestamp);
    }

private:
    TDocs<128> EvaluatePredicates(
        const std::vector<std::vector<bool>>& predicates,
        const std::vector<TDocs<128>>& bitSliceIndex
    ) {
        TDocs<128> docs;

        for (const auto& predicate: predicates) {
            docs.Or(EvaluatePredicate(predicate, bitSliceIndex));
        }

        return docs;
    }

    TDocs<128> EvaluatePredicate(
        const std::vector<bool>& predicate,
        const std::vector<TDocs<128>>& bitSliceIndex
    ) {
        TDocs<128> docs = AddedDocs;

        for (size_t i = 0; i < predicate.size(); ++i) {
            if (predicate[i]) {
                docs.And(bitSliceIndex[i]);
            } else {
                docs.And(bitSliceIndex[i].Not());
            }
        }

        return docs;
    }

private:
    TDocs<128> AddedDocs;
    std::vector<TDocs<128>> DocIDsByBitStart;
    std::vector<TDocs<128>> DocIDsByBitEnd;
};
