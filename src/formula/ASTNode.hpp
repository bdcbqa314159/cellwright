#pragma once
#include "core/CellAddress.hpp"
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace magic {

struct NumberNode   { double value; };
struct StringNode   { std::string value; };
struct BoolNode     { bool value; };
struct CellRefNode  { CellAddress addr; };
struct RangeNode    { CellAddress from; CellAddress to; };

struct ASTNode;
using ASTNodePtr = std::unique_ptr<ASTNode>;

struct UnaryOpNode  { char op; ASTNodePtr operand; };
struct BinOpNode    { char op; ASTNodePtr left; ASTNodePtr right; };
struct FuncCallNode { std::string name; std::vector<ASTNodePtr> args; };

// A single comparison operator stored as string to support <=, >=, <>
struct CompareNode  { std::string op; ASTNodePtr left; ASTNodePtr right; };

struct ASTNode {
    using Value = std::variant<
        NumberNode, StringNode, BoolNode,
        CellRefNode, RangeNode,
        UnaryOpNode, BinOpNode, FuncCallNode, CompareNode
    >;
    Value value;

    template <typename T>
    ASTNode(T&& v) : value(std::forward<T>(v)) {}
};

inline ASTNodePtr make_node(auto&& v) {
    return std::make_unique<ASTNode>(std::forward<decltype(v)>(v));
}

}  // namespace magic
