#include <gtest/gtest.h>

#include "text_processor.h"
#include "inverted_index.h"

using namespace NLogicAlgebra;

TEST(LogicAlgebra, Parser) {
    auto ast = And("alexei", "n", "cool", Or("uchitel", "lol"));
    ASSERT_EQ(ast->ChildrenView().size(), 4);
    ASSERT_EQ(ast->Child(3)->ChildrenView().size(), 2);
}

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
    ASSERT_EQ(index.FindDocsByWord("Putin").GetIDs(), expected);
    expected = {1, 3, 4};
    ASSERT_EQ(index.FindDocsByWord("eUroPe").GetIDs(), expected);
    expected = {0};
    ASSERT_EQ(index.FindDocsByWord("Podnebesny").GetIDs(), expected);
    expected = {0, 1, 2, 3};
    ASSERT_EQ(index.FindDocsByWord("russia").GetIDs(), expected);

    expected = {0, 1, 3, 4};
    ASSERT_EQ(index.FindDocsByExpr(Or("Podnebesny", "eUroPe")).GetIDs(), expected);
    expected = {0, 1};
    ASSERT_EQ(index.FindDocsByExpr(And("russia", Or("Putin", "Podnebesny"))).GetIDs(), expected);
}

TDocument CreateDocument(const std::string& text) {
    static std::size_t COUNTER = 0;
    return TDocument{.ID = COUNTER++, .Text = text};
}

TEST(TInvertedPatternIndex, Basic) {
    std::filesystem::remove_all("./test");
    std::filesystem::create_directory("./test");

    TInvertedPatternIndex<128> index("./test");
    index.AddDocument(CreateDocument("hello world"));
    index.AddDocument(CreateDocument("hell world"));

    std::vector<std::size_t> expected = {0, 1};
    ASSERT_EQ(index.FindDocsByPattern("*hell*worl*").GetIDs(), expected);
    ASSERT_EQ(index.FindDocsByPattern("hell*worl*").GetIDs(), expected);
    ASSERT_EQ(index.FindDocsByPrefix("hell").GetIDs(), expected);
    expected = {0};
    ASSERT_EQ(index.FindDocsByPrefix("hello").GetIDs(), expected);
    expected = {};
    ASSERT_EQ(index.FindDocsByPattern("ell*worl*").GetIDs(), expected);
    ASSERT_EQ(index.FindDocsByPrefix("ell").GetIDs(), expected);
}
