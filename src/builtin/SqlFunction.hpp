#pragma once

namespace magic {
class FunctionRegistry;
class DuckDBEngine;
class Workbook;

void register_sql_function(FunctionRegistry& registry, DuckDBEngine& engine, Workbook& workbook);
}  // namespace magic
