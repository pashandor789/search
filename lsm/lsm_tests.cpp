#include <gtest/gtest.h>
#include <random>
#include "lsm.h"

#include "spdlog/spdlog.h"

TEST(LSMTree, CorrectnessPointRequest) {
    const size_t DATA_SIZE = 1'024'000;

    const int64_t MIN_RAND = -1'000'000;
    const int64_t MAX_RAND = 1'000'000;

    std::filesystem::remove_all("./test");
    std::filesystem::create_directory("./test");
    TLSMTree<size_t, int> lsm("./test");

    std::random_device rd;
    std::mt19937 g(rd());

    std::vector<std::pair<size_t, int>> testData;
    std::uniform_int_distribution<int> dist(MIN_RAND, MAX_RAND);
    for (size_t i = 0; i < DATA_SIZE; ++i) {
        testData.emplace_back(i, dist(g));
    }
    std::shuffle(testData.begin(), testData.end(), g);

    for (int i = 0; i < DATA_SIZE; ++i) {
        lsm.Insert(testData[i].first, testData[i].second);
        if (i % 10'000 == 0) {
            auto entry = lsm.ReadPoint(testData[i].first);
            ASSERT_TRUE(entry.has_value()) << "not found: " << i;;
            ASSERT_EQ(entry.value().second, testData[i].second);
        }
    }

    std::shuffle(testData.begin(), testData.end(), g);
    const size_t POINT_REQUEST_SIZE = 10'000;
    for (int i = 0; i < POINT_REQUEST_SIZE; ++i) {
        auto entry = lsm.ReadPoint(testData[i].first);
        ASSERT_TRUE(entry.has_value()) << "not found: " << i;
        ASSERT_EQ(entry.value().second, testData[i].second);
    }

    size_t BLOOM_FILTER_CHECK = 10'000;
    for (size_t i = 0; i < BLOOM_FILTER_CHECK; ++i) {
        int64_t key = abs(dist(g)) + DATA_SIZE;
        ASSERT_FALSE(lsm.ReadPoint(key).has_value()) << key << " found, but it mustn't there";
    }
}

TEST(LSMTree, CorrectnessRangeRequest) {
    const int DATA_SIZE = 1'024'000;

    const int64_t MIN_RAND = -1'000'000;
    const int64_t MAX_RAND = 1'000'000;

    std::filesystem::remove_all("./test");
    std::filesystem::create_directory("./test");
    TLSMTree<int, int> lsm("./test");

    std::random_device rd;
    std::mt19937 g(rd());

    std::vector<std::pair<int, int>> testData;
    std::uniform_int_distribution<int> dist(MIN_RAND, MAX_RAND);
    for (int i = -DATA_SIZE / 2; i < DATA_SIZE / 2; ++i) {
        testData.emplace_back(i, dist(g));
    }

    for (const auto& [k, v]: testData) {
        lsm.Insert(k, v);
    }

    ASSERT_TRUE(lsm.ReadRanges(-3 * DATA_SIZE, -2 * DATA_SIZE).empty());
    ASSERT_TRUE(lsm.ReadRanges(2 * DATA_SIZE, 3 * DATA_SIZE).empty());

    const size_t RANGE_REQUEST_SIZE = 10;
    for (int i = 0; i < RANGE_REQUEST_SIZE; ++i) {
        auto lhs = testData[rand() % testData.size()];
        auto rhs = testData[rand() % testData.size()];
        if (lhs.first > rhs.first) { std::swap(lhs, rhs); }

        auto range = lsm.ReadRanges(lhs.first, rhs.second);
    }
}
