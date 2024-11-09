#pragma once

#include "docs.h"
#include <memory>

namespace NLogicAlgebra {
    class IASTNode {
    public:
        IASTNode(std::vector<std::shared_ptr<IASTNode>> children)
                : Children(std::move(children))
        {}

        IASTNode() = default;

        virtual ~IASTNode() = default;

    public:
        struct TContext {
            TContext(const std::function<TDocs<128>(std::string)> & findDocsByWord)
                : FindDocsByWord(findDocsByWord)
            {}

            std::function<TDocs<128>(std::string)> FindDocsByWord;
        };

        virtual TDocs<128> Evaluate(TContext& ctx) = 0;

        const std::vector<std::shared_ptr<IASTNode>>& ChildrenView() const {
            return Children;
        }

        std::shared_ptr<IASTNode> Child(size_t idx) const {
            assert(idx < Children.size());
            return Children[idx];
        }

    protected:
        std::vector<std::shared_ptr<IASTNode>> Children;
    };

    class TLiteral : public IASTNode {
    public:
        TLiteral(std::string word)
                : Word(std::move(word))
        {}

        TDocs<128> Evaluate(TContext& ctx) override {
            auto res = ctx.FindDocsByWord(Word);
            return res;
        }

    private:
        std::string Word;
    };

    class TAnd : public IASTNode {
    public:
        TAnd(std::vector<std::shared_ptr<IASTNode>> children)
                : IASTNode(std::move(children))
        {}

        TDocs<128> Evaluate(TContext& ctx) override {
            TDocs<128> result;
            result.SetAll();

            for (auto& child: this->Children) {
                if (child == nullptr) continue;
                result.And(child->Evaluate(ctx));
            }
            return result;
        }
    };

    class TOr : public IASTNode {
    public:
        TOr(std::vector<std::shared_ptr<IASTNode>> children)
                : IASTNode(std::move(children))
        {}

        TDocs<128> Evaluate(TContext& ctx) override {
            TDocs<128> result;
            for (auto& child: this->Children) {
                if (child == nullptr) continue;
                result.Or(child->Evaluate(ctx));
            }
            return result;
        }
    };

    // user expressions
    enum EOperation : uint64_t {
        EAnd = 0,
        EOr = 1
    };

    void processArguments(std::vector<std::shared_ptr<IASTNode>>& nodeCollection) {
    }

    template <typename T, typename... Args>
    void processArguments(std::vector<std::shared_ptr<IASTNode>>& nodeCollection, T arg, Args... args) {
        if constexpr (std::is_same_v<T, std::shared_ptr<IASTNode>>) {
            nodeCollection.push_back(arg);
        } else if constexpr (std::is_convertible_v<T, std::string>) {
            nodeCollection.push_back(std::make_shared<TLiteral>(std::move(arg)));
        }

        processArguments(nodeCollection, args...);
    }

    template <typename... Args>
    std::shared_ptr<IASTNode> Operation(EOperation operation, Args... args) {
        std::vector<std::shared_ptr<IASTNode>> nodeCollection;

        processArguments(nodeCollection, args...);

        switch (operation) {
            case EOperation::EAnd: { return std::make_shared<TAnd>(std::move(nodeCollection)); }
            case EOperation::EOr: { return std::make_shared<TOr>(std::move(nodeCollection)); }
            default: throw std::runtime_error("no such operation.");
        }

        std::unreachable();
    }

    template <typename... Args>
    std::shared_ptr<IASTNode> And(Args... args) {
        return Operation(EOperation::EAnd, args...);
    } // And("asdf", "sadfsdaf, "sadfasdf")

    template <typename... Args>
    std::shared_ptr<IASTNode> Or(Args... args) {
        return Operation(EOperation::EOr, args...);
    }
}