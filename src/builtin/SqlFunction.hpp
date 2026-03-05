#pragma once

namespace magic {
class FunctionRegistry;
class DuckDBEngine;
class Sheet;

void register_sql_function(FunctionRegistry& registry, DuckDBEngine& engine, Sheet& sheet);
}  // namespace magic
