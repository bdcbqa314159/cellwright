#include "core/ArrowBridge.hpp"
#include "core/CellAddress.hpp"
#include "core/CellValue.hpp"
#include "core/Sheet.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#ifndef ARROW_FLAG_NULLABLE
#define ARROW_FLAG_NULLABLE 2
#endif

namespace magic {

// ── Validity bitmap helpers ─────────────────────────────────────

static size_t bitmap_byte_count(int64_t n) {
    return static_cast<size_t>((n + 7) / 8);
}

static void bitmap_set(uint8_t* bitmap, int64_t i) {
    bitmap[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
}

static bool bitmap_get(const uint8_t* bitmap, int64_t i) {
    return (bitmap[i / 8] >> (i % 8)) & 1;
}

// ── Private data for release callbacks ──────────────────────────

// Holds allocated resources for a child (column-level) ArrowArray.
struct ColumnExportData {
    std::vector<uint8_t> validity;
    // buffers[0] = validity bitmap, buffers[1] = data pointer (into Column)
    const void* buffers[2];
};

// Holds allocated resources for the top-level struct ArrowArray.
struct StructExportData {
    std::vector<std::unique_ptr<ColumnExportData>> col_data;
    std::vector<ArrowArray> child_arrays;
    std::vector<ArrowArray*> child_ptrs;
    std::vector<ArrowSchema> child_schemas;
    std::vector<ArrowSchema*> child_schema_ptrs;
    std::vector<std::string> col_names;
    // Struct-level buffers (one null pointer for the validity bitmap)
    const void* struct_buffers[1];
};

// ── Release callbacks ───────────────────────────────────────────

static void release_child_array(ArrowArray* array) {
    // private_data is managed by the parent StructExportData
    array->release = nullptr;
}

static void release_struct_array(ArrowArray* array) {
    if (array->private_data) {
        delete static_cast<StructExportData*>(array->private_data);
        array->private_data = nullptr;
    }
    array->release = nullptr;
}

static void release_child_schema(ArrowSchema* schema) {
    // name is owned by the parent StructExportData
    schema->release = nullptr;
}

static void release_struct_schema(ArrowSchema* schema) {
    // children array and child schemas are owned by StructExportData
    // which is freed by release_struct_array
    if (schema->children) {
        schema->children = nullptr;
    }
    schema->release = nullptr;
}

// ── Export ───────────────────────────────────────────────────────

void export_sheet_to_arrow(const Sheet& sheet, ArrowSchema* schema, ArrowArray* array) {
    int32_t num_cols = sheet.col_count();
    int32_t num_rows = sheet.row_count();

    auto data = std::make_unique<StructExportData>();
    data->col_data.resize(static_cast<size_t>(num_cols));
    data->child_arrays.resize(static_cast<size_t>(num_cols));
    data->child_ptrs.resize(static_cast<size_t>(num_cols));
    data->child_schemas.resize(static_cast<size_t>(num_cols));
    data->child_schema_ptrs.resize(static_cast<size_t>(num_cols));
    data->col_names.resize(static_cast<size_t>(num_cols));

    for (int32_t c = 0; c < num_cols; ++c) {
        auto ci = static_cast<size_t>(c);
        const auto& col = sheet.column(c);
        const auto& doubles = col.doubles();

        // Build validity bitmap: valid where doubles[i] is not NaN
        auto cd = std::make_unique<ColumnExportData>();
        cd->validity.resize(bitmap_byte_count(num_rows), 0);

        int64_t null_count = 0;
        for (int32_t r = 0; r < num_rows; ++r) {
            if (r < static_cast<int32_t>(doubles.size()) && !std::isnan(doubles[r])) {
                bitmap_set(cd->validity.data(), r);
            } else {
                ++null_count;
            }
        }

        // Point directly at Column's contiguous double storage (zero-copy)
        cd->buffers[0] = cd->validity.data();
        cd->buffers[1] = doubles.data();

        // Fill child ArrowArray
        ArrowArray& child = data->child_arrays[ci];
        child.length = num_rows;
        child.null_count = null_count;
        child.offset = 0;
        child.n_buffers = 2;
        child.n_children = 0;
        child.buffers = cd->buffers;
        child.children = nullptr;
        child.dictionary = nullptr;
        child.release = release_child_array;
        child.private_data = cd.get();

        data->col_data[ci] = std::move(cd);
        data->child_ptrs[ci] = &data->child_arrays[ci];

        // Fill child ArrowSchema
        data->col_names[ci] = CellAddress::col_to_letters(c);
        ArrowSchema& cs = data->child_schemas[ci];
        cs.format = "g";  // float64
        cs.name = data->col_names[ci].c_str();
        cs.metadata = nullptr;
        cs.flags = ARROW_FLAG_NULLABLE;
        cs.n_children = 0;
        cs.children = nullptr;
        cs.dictionary = nullptr;
        cs.release = release_child_schema;
        cs.private_data = nullptr;

        data->child_schema_ptrs[ci] = &data->child_schemas[ci];
    }

    // Fill top-level struct ArrowArray
    data->struct_buffers[0] = nullptr;  // no validity bitmap for struct
    array->length = num_rows;
    array->null_count = 0;
    array->offset = 0;
    array->n_buffers = 1;
    array->n_children = num_cols;
    array->buffers = data->struct_buffers;
    array->children = data->child_ptrs.data();
    array->dictionary = nullptr;
    array->release = release_struct_array;
    array->private_data = data.get();

    // Fill top-level struct ArrowSchema
    schema->format = "+s";  // struct
    schema->name = "";
    schema->metadata = nullptr;
    schema->flags = 0;
    schema->n_children = num_cols;
    schema->children = data->child_schema_ptrs.data();
    schema->dictionary = nullptr;
    schema->release = release_struct_schema;
    schema->private_data = nullptr;

    // Transfer ownership to the ArrowArray
    (void)data.release();
}

// ── Import ──────────────────────────────────────────────────────

Sheet import_sheet_from_arrow(
    const ArrowSchema* schema,
    const ArrowArray* array,
    const std::string& sheet_name)
{
    assert(schema && array);
    assert(std::strcmp(schema->format, "+s") == 0 && "Expected struct schema");

    int64_t num_cols = schema->n_children;
    int64_t num_rows = array->length;

    Sheet sheet(sheet_name,
                static_cast<int32_t>(num_cols),
                static_cast<int32_t>(num_rows));

    for (int64_t c = 0; c < num_cols; ++c) {
        const ArrowSchema* cs = schema->children[c];
        const ArrowArray* ca = array->children[c];

        // Set column name on the sheet if available
        if (cs->name && cs->name[0] != '\0') {
            // Column names are used for display; sheet columns are indexed
        }

        if (std::strcmp(cs->format, "g") != 0) {
            // Skip non-float64 columns — future extension point
            continue;
        }

        const auto* validity = (ca->n_buffers >= 1 && ca->buffers[0])
            ? static_cast<const uint8_t*>(ca->buffers[0])
            : nullptr;
        const auto* values = (ca->n_buffers >= 2)
            ? static_cast<const double*>(ca->buffers[1])
            : nullptr;

        if (!values) continue;

        int64_t offset = ca->offset;
        auto col_idx = static_cast<int32_t>(c);

        for (int64_t r = 0; r < num_rows; ++r) {
            int64_t idx = r + offset;
            bool valid = !validity || bitmap_get(validity, idx);
            if (valid) {
                sheet.set_value({col_idx, static_cast<int32_t>(r)},
                                CellValue{values[idx]});
            }
            // Invalid (null) entries remain as empty (monostate)
        }
    }

    return sheet;
}

}  // namespace magic
