#include <gtest/gtest.h>

#include "text_processor.h"
#include "inverted_index.h"

std::string Join(const std::vector<std::string>& words) {
    std::ostringstream stream;
    for (size_t i = 0; i < words.size(); ++i) {
        stream << words[i];
        if (i != words.size() - 1) {
            stream << " ";
        }
    }
    return stream.str();
}


TEST(TextProcessor, Basic) {
    TTextProcessor processor;
    std::string s = "give me the documentation of the having sex plz";
    ASSERT_EQ("give me document have sex plz", Join(processor.Process(s)));
}

TDocument GetDocument(std::size_t docID) {
    std::ifstream file(std::filesystem::current_path() / "static" / "documents" /  std::to_string(docID));
    if (!file.is_open()) {
        EXPECT_TRUE(false) << "can't find document #" << docID;
        exit(1);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    file.close();
    return TDocument{.ID = docID, .Text = std::move(content)};
}

TEST(InvertedIndex, Basic) {
    std::filesystem::remove_all("./test");
    std::filesystem::create_directory("./test");

    TInvertedIndex<128> index("./test");
    for (size_t i = 0; i < 5; ++i) {
        index.AddDocument(GetDocument(i));
    }

    std::vector<std::size_t> expected = {1};
    ASSERT_EQ(index.FindDocsByWord("Putin"), expected);
    expected = {1, 3, 4};
    ASSERT_EQ(index.FindDocsByWord("eUroPe"), expected);
    expected = {0};
    ASSERT_EQ(index.FindDocsByWord("Podnebesny"), expected);
    expected = {0, 1, 2, 3};
    ASSERT_EQ(index.FindDocsByWord("russia"), expected);
}
