#pragma once

#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>
#include <string>
#include <printf.h>
#include <vector>

#include "spdlog/spdlog.h"

namespace NSSTable {
    // Helper struct to check if TKey is hashable
    template <typename TKey, typename = void>
    struct is_hashable : std::false_type {};

    template <typename TKey>
    struct is_hashable<TKey, std::void_t<decltype(std::hash<TKey>{}(std::declval<TKey>()))>> : std::true_type {};

    template <typename TKey>
    class TBloomFilter {
    public:
        TBloomFilter(std::size_t size = 1'024, size_t hashCount = 3)
            : BitArray(size)
            , HashCount(hashCount)
        {}

        void Count(const TKey& item) {
            for (size_t i = 0; i < HashCount; ++i) {
                BitArray[Hash(item, i) % BitArray.size()] = true;
            }
        }

        bool Probe(const TKey& item) const {
            for (size_t i = 0; i < HashCount; ++i) {
                if (!BitArray[Hash(item, i) % BitArray.size()]) {
                    return false;
                }
            }

            return true;
        }

        void Reset() {
            BitArray.assign(BitArray.size(), false);
        }

    private:
        std::vector<bool> BitArray;
        std::size_t HashCount;

        std::size_t Hash(const TKey& item, size_t seed) const {
            if constexpr (is_hashable<TKey>::value) {
                return std::hash<TKey>()(item) + seed * 0x9e3779b9;
            } else {
                std::string s(reinterpret_cast<const char *>(&item), sizeof(item));
                return std::hash<std::string>()(s) + seed * 0x9e3779b9;
            }
        }
    };

    template<typename TKey>
    struct TMeta {
        std::size_t Size{};
        TBloomFilter<TKey> BloomFilter{};
    };
}

template <typename TKey, typename TValue>
class TMemTable {
public:
    using TEntry = std::pair<TKey, TValue>;
    const static std::size_t MAX_SIZE = 10'240ull;

public:
    explicit TMemTable()
        : BloomFilter(MAX_SIZE * 4) {
        Data.reserve(MAX_SIZE);
    }

    void Insert(TKey key, TValue value) {
        BloomFilter.Count(key);
        Data.push_back({std::move(key), std::move(value)});
    }

    std::optional<TEntry> ReadPoint(const TKey& key) const {
        if (!BloomFilter.Probe(key)) {
            return std::nullopt;
        }

        auto it = std::find_if(Data.rbegin(), Data.rend(), [&key](const TEntry& e){ return e.first == key; });
        if (it == Data.rend()) {
            return std::nullopt;
        }
        return *it;
    }

    NSSTable::TMeta<TKey> DumpAsSSTable(const std::filesystem::path& path) {
        std::set<TEntry> uniqueLastOccur(std::make_move_iterator(Data.rbegin()), std::make_move_iterator(Data.rend()));
        Data = {std::make_move_iterator(uniqueLastOccur.begin()), std::make_move_iterator(uniqueLastOccur.end())};

        NSSTable::TMeta<TKey> metaData = {.Size = Data.size(), .BloomFilter = BloomFilter};
        std::ofstream fOut(path, std::ios::out | std::ios::binary);
        fOut.write(reinterpret_cast<const char*>(Data.data()), Data.size() * sizeof(TEntry));
        Data.clear();
        BloomFilter.Reset();

        return metaData;
    }

    std::size_t Size() const {
        return Data.size();
    }

private:
    NSSTable::TBloomFilter<TKey> BloomFilter{};
    std::vector<TEntry> Data{};
};

template <typename TKey, typename TValue>
class TLSMTree {
public:
    using TEntry = std::pair<TKey, TValue>;

    struct TMeta {
        std::size_t SSTableDiffCoefficient = 3;
        std::vector<NSSTable::TMeta<TKey>> SSTableMeta{};
    };

    struct TStatistics {
        std::size_t BloomFilterReadPointFalsePositive = 0;
        std::size_t BloomFilterReadPointLookupCount = 0;
        std::size_t LookUpCount = 0;
        std::size_t MemTableSuccessLookupCount = 0;
        std::size_t InsertCount = 0;

        ~TStatistics() {
            std::stringstream ss;
            ss
                << "LSMTree Statistics: \n\t"
                << "LookUpCount: " << LookUpCount << "\n\t"
                << "MemTableSuccessLookupCount: " << MemTableSuccessLookupCount << "\n\t"
                << "BloomFilterReadPointFalsePositive: " << BloomFilterReadPointFalsePositive << "\n\t"
                << "BloomFilterReadPointLookupCount: " << BloomFilterReadPointLookupCount << "\n\t"
                << "InsertCount: " << InsertCount << "\n\t";

            std::cerr << ss.str();
        }
    };

public:
    TLSMTree(std::filesystem::path sourcePath)
        : SourcePath(std::move(sourcePath))
    {
        LoadFromDisk();
    }

    void Insert(TKey key, TValue value) {
        ++Stats.InsertCount;
        MemTable.Insert(std::move(key), std::move(value));
        if (MemTable.Size() == TMemTable<TKey, TValue>::MAX_SIZE) {
            auto ssTableMeta = MemTable.DumpAsSSTable(GetSSTablePath(MetaData.SSTableMeta.size()));
            MetaData.SSTableMeta.push_back(std::move(ssTableMeta));
            CompactSSTables();
        }
    }

    template<bool LeftBinSearch = true>
    std::streamoff SSTableExternalMemoryBinSearch(
            std::ifstream& ssTableFile,
            const std::function<bool(const TEntry&)>& good
    ) const {
        ssTableFile.seekg(0, std::ios::end);
        std::streamoff fileSize = ssTableFile.tellg();
        ssTableFile.seekg(0);
        std::streamoff numPairs = fileSize / sizeof(TEntry);

        std::streamoff l = -1;
        std::streamoff r = numPairs;

        while (r - l > 1) {
            std::streamoff mid = (l + r) / 2;

            ssTableFile.seekg(mid * sizeof(TEntry));
            TEntry midEntry; ssTableFile.read(reinterpret_cast<char*>(&midEntry), sizeof(midEntry));

            if constexpr (LeftBinSearch) {
                if (good(midEntry)) {
                    l = mid;
                } else {
                    r = mid;
                }
            } else {
                if (good(midEntry)) {
                    r = mid;
                } else {
                    l = mid;
                }
            }
        }

        if constexpr (LeftBinSearch) {
            return l;
        } else {
            return r == numPairs? -1: r;
        }
    }

    std::vector<TEntry> ReadPoints(const std::vector<TKey>& keys) const {
        std::vector<TEntry> res;
        res.reserve(keys.size());

        for (const auto& key: keys) {
            if (auto maybeEntry = ReadPoint(key)) {
                res.push_back(std::move(maybeEntry.value()));
            }
        }

        return res;
    }

    std::optional<TEntry> ReadPoint(const TKey& key) const {
        ++Stats.LookUpCount;

        if (auto entry = MemTable.ReadPoint(key)) {
            ++Stats.MemTableSuccessLookupCount;
            return entry;
        }

        for (int i = MetaData.SSTableMeta.size() - 1; i >= 0; --i) {
            ++Stats.BloomFilterReadPointLookupCount;
            if (!MetaData.SSTableMeta[i].BloomFilter.Probe(key)) {
                continue;
            }

            std::ifstream fIn(GetSSTablePath(i), std::ios::binary);
            auto maybeEntryPos = SSTableExternalMemoryBinSearch(fIn, [&key](const TEntry& mid){ return mid.first <= key; });
            fIn.seekg(maybeEntryPos * sizeof(TEntry));
            TEntry entry; fIn.read(reinterpret_cast<char*>(&entry), sizeof(entry));
            if (entry.first == key) {
                return entry;
            }

            ++Stats.BloomFilterReadPointFalsePositive;
        }

        return std::nullopt;
    }

    std::vector<TEntry> ReadRanges(const TKey& lhs, const TKey& rhs) const {
        std::vector<TEntry> result;

        for (size_t i = 0; i < MetaData.SSTableMeta.size(); ++i) {
            std::ifstream fIn(GetSSTablePath(i), std::ios::binary);
            auto lRangePos = SSTableExternalMemoryBinSearch<false>(fIn, [&lhs](const TEntry& mid){ return lhs <= mid.first; });
            if (lRangePos < 0) {
                continue;
            }
            auto rRangePos = SSTableExternalMemoryBinSearch<true >(fIn, [&rhs](const TEntry& mid){ return mid.first <= rhs; });
            if (rRangePos < 0) {
                continue;
            }
            auto tmp = ScanRange(fIn, lRangePos, rRangePos);
            result.insert(result.end(), tmp.begin(), tmp.end());
        }

        return result;
    }

private:
    std::vector<TEntry> ScanRange(std::ifstream& fIn, std::streampos lRangePos, std::streampos rRangePos) const {
        std::vector<TEntry> result;

        fIn.seekg(lRangePos * sizeof(TEntry));
        while (fIn.tellg() <= rRangePos * sizeof(TEntry)) {
            TEntry entry; fIn.read(reinterpret_cast<char*>(&entry), sizeof(entry));
            result.push_back(std::move(entry));
        }

        return result;
    }

    void LoadFromDisk() {
        std::filesystem::path MetaDataPath = SourcePath / "meta";

        if (!std::filesystem::exists(MetaDataPath)) {
            spdlog::info("There is no LSM Tree on the disk.");
            return;
        }

        std::ifstream fIn(MetaDataPath, std::ios::in | std::ios::binary);
        spdlog::info("Loading LSM meta data from the disk.");
        fIn.read(reinterpret_cast<char*>(&MetaData), sizeof(MetaData));
    }

    void CompactSSTables() {
        spdlog::debug("Compacting SSTables.");

        size_t beforeSize = MetaData.SSTableMeta.size();
        for (size_t i = MetaData.SSTableMeta.size() - 1; i != 0; --i) {
            if (MetaData.SSTableDiffCoefficient * MetaData.SSTableMeta[i].Size > MetaData.SSTableMeta[i - 1].Size) {
                auto mergedSSTableMeta = MergeSSTables(i, i - 1);
                MetaData.SSTableMeta.pop_back();
                MetaData.SSTableMeta[i - 1] = mergedSSTableMeta;
            }
        }

        spdlog::debug("Before the compaction: " + std::to_string(beforeSize) + ", after the compaction: " + std::to_string(MetaData.SSTableMeta.size()));
    }

    NSSTable::TMeta<TKey> MergeSSTables(size_t lhsInd, size_t rhsInd) {
        assert(lhsInd > rhsInd);

        auto& lhsMeta = MetaData.SSTableMeta[lhsInd];
        auto& rhsMeta = MetaData.SSTableMeta[rhsInd];
        NSSTable::TBloomFilter<TKey> bloomFilter((lhsMeta.Size + rhsMeta.Size) * 5);

        assert(lhsMeta.Size <= rhsMeta.Size);

        std::ifstream fInLhs(GetSSTablePath(lhsInd), std::ios::binary);
        std::ifstream fInRhs(GetSSTablePath(rhsInd), std::ios::binary);
        std::ofstream fOut(SourcePath / "tmp", std::ios::binary);

        size_t resSize = 0;

        TEntry lhs; fInLhs.read(reinterpret_cast<char*>(&lhs), sizeof(lhs));
        TEntry rhs; fInRhs.read(reinterpret_cast<char*>(&rhs), sizeof(rhs));
        for (;!fInLhs.eof() && !fInRhs.eof(); ++resSize) {
            if (lhs.first <= rhs.first) {
                bloomFilter.Count(lhs.first);
                fOut.write(reinterpret_cast<const char*>(&lhs), sizeof(lhs));
                fInLhs.read(reinterpret_cast<char*>(&lhs), sizeof(lhs));
            } else if (lhs.first > rhs.first) {
                bloomFilter.Count(rhs.first);
                fOut.write(reinterpret_cast<const char*>(&rhs), sizeof(rhs));
                fInRhs.read(reinterpret_cast<char*>(&rhs), sizeof(rhs));
            }
        }

        for (;!fInLhs.eof(); ++resSize) {
            bloomFilter.Count(lhs.first);
            fOut.write(reinterpret_cast<const char*>(&lhs), sizeof(lhs));
            fInLhs.read(reinterpret_cast<char*>(&lhs), sizeof(lhs));
        }

        for (;!fInRhs.eof(); ++resSize) {
            bloomFilter.Count(rhs.first);
            fOut.write(reinterpret_cast<const char*>(&rhs), sizeof(rhs));
            fInRhs.read(reinterpret_cast<char*>(&rhs), sizeof(rhs));
        }

        std::filesystem::rename(SourcePath / "tmp", GetSSTablePath(rhsInd));
        std::filesystem::remove(GetSSTablePath(lhsInd));

        return NSSTable::TMeta<TKey>{.Size = resSize, .BloomFilter = std::move(bloomFilter)};
    }

    std::filesystem::path GetSSTablePath(size_t index) const {
        return SourcePath / ("C" + std::to_string(index));
    }

private:
    TMemTable<TKey, TValue> MemTable{};
    TMeta MetaData{};
    std::filesystem::path SourcePath{};
    mutable TStatistics Stats{};
};

//
