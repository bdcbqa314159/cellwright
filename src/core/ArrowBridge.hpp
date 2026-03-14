#pragma once

#include <cstdint>
#include <string>

// ── Arrow C Data Interface structs (from the Arrow spec) ────────
// These are ABI-stable C structs — no Arrow C++ library dependency.

#ifndef ARROW_C_DATA_INTERFACE
#define ARROW_C_DATA_INTERFACE

struct ArrowSchema {
    const char* format;
    const char* name;
    const char* metadata;
    int64_t flags;
    int64_t n_children;
    struct ArrowSchema** children;
    struct ArrowSchema* dictionary;
    void (*release)(struct ArrowSchema*);
    void* private_data;
};

struct ArrowArray {
    int64_t length;
    int64_t null_count;
    int64_t offset;
    int64_t n_buffers;
    int64_t n_children;
    const void** buffers;
    struct ArrowArray** children;
    struct ArrowArray* dictionary;
    void (*release)(struct ArrowArray*);
    void* private_data;
};

#endif  // ARROW_C_DATA_INTERFACE

namespace magic {

class Sheet;

// Export a Sheet as an Arrow struct array (record batch).
// The ArrowArray holds zero-copy pointers into the Sheet's Column data,
// so the Sheet must outlive the exported arrays.
// Caller must eventually call schema->release(schema) and array->release(array).
void export_sheet_to_arrow(const Sheet& sheet, ArrowSchema* schema, ArrowArray* array);

// Import an Arrow struct array (record batch) into a new Sheet.
// Copies data into cellwright's columnar storage.
[[nodiscard]] Sheet import_sheet_from_arrow(
    const ArrowSchema* schema,
    const ArrowArray* array,
    const std::string& sheet_name = "Imported");

}  // namespace magic
