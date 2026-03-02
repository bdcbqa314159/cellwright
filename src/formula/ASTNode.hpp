#pragma once
#include "core/Arena.hpp"
#include "core/CellAddress.hpp"
#include <string>
#include <variant>
#include <vector>

namespace magic {

struct NumberNode   { double value; };
struct StringNode   { std::string value; };
struct BoolNode     { bool value; };
struct CellRefNode  { CellAddress addr; };
struct RangeNode    { CellAddress from; CellAddress to; };
struct SheetRefNode { std::string sheet_name; CellAddress addr; };
struct SheetRangeNode { std::string sheet_name; CellAddress from; CellAddress to; };

struct ASTNode;

struct UnaryOpNode  { char op; ASTNode* operand; };
struct BinOpNode    { char op; ASTNode* left; ASTNode* right; };
struct FuncCallNode { std::string name; std::vector<ASTNode*> args; };

// A single comparison operator stored as string to support <=, >=, <>
struct CompareNode  { std::string op; ASTNode* left; ASTNode* right; };

struct ASTNode {
    using Value = std::variant<
        NumberNode, StringNode, BoolNode,
        CellRefNode, RangeNode, SheetRefNode, SheetRangeNode,
        UnaryOpNode, BinOpNode, FuncCallNode, CompareNode
    >;
    Value value;

    template <typename T>
    ASTNode(T&& v) : value(std::forward<T>(v)) {}
};

inline ASTNode* make_node(Arena& arena, auto&& v) {
    return arena.create<ASTNode>(std::forward<decltype(v)>(v));
}

// Owns an Arena and the root AST node allocated within it.
// When destroyed, all nodes are freed in one bulk deallocation.
struct ParsedFormula {
    Arena arena;
    ASTNode* root = nullptr;
    explicit operator bool() const { return root != nullptr; }
};

}  // namespace magic
