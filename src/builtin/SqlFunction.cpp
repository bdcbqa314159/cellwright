#include "builtin/SqlFunction.hpp"
#include "core/DuckDBEngine.hpp"
#include "core/Workbook.hpp"
#include "formula/FunctionRegistry.hpp"

namespace magic {

void register_sql_function(FunctionRegistry& registry, DuckDBEngine& engine, Workbook& workbook) {
    registry.register_function("SQL",
        [&engine, &workbook](const std::vector<CellValue>& args) -> CellValue {
            if (args.empty() || !is_string(args[0]))
                return CellValue{CellError::VALUE};

            const std::string& sql = as_string(args[0]);

            // Import active sheet as "data" table
            engine.import_sheet(workbook.active_sheet(), "data");

            auto result = engine.query(sql);
            if (!result.ok())
                return CellValue{CellError::VALUE};

            // Return first cell of result
            if (result.row_count() > 0 && result.col_count() > 0)
                return result.columns[0][0];

            return CellValue{};
        });
}

}  // namespace magic
