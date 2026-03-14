"""Test Arrow C Data Interface interop with pyarrow, polars, and pandas."""

import ctypes
import struct
from pathlib import Path

import numpy as np
import pandas as pd
import polars as pl
import pyarrow as pa


# ── Arrow C Data Interface structs via ctypes ────────────────────

class ArrowSchema(ctypes.Structure):
    pass

ArrowSchema._fields_ = [
    ("format", ctypes.c_char_p),
    ("name", ctypes.c_char_p),
    ("metadata", ctypes.c_char_p),
    ("flags", ctypes.c_int64),
    ("n_children", ctypes.c_int64),
    ("children", ctypes.POINTER(ctypes.POINTER(ArrowSchema))),
    ("dictionary", ctypes.POINTER(ArrowSchema)),
    ("release", ctypes.CFUNCTYPE(None, ctypes.POINTER(ArrowSchema))),
    ("private_data", ctypes.c_void_p),
]


class ArrowArray(ctypes.Structure):
    pass

ArrowArray._fields_ = [
    ("length", ctypes.c_int64),
    ("null_count", ctypes.c_int64),
    ("offset", ctypes.c_int64),
    ("n_buffers", ctypes.c_int64),
    ("n_children", ctypes.c_int64),
    ("buffers", ctypes.POINTER(ctypes.c_void_p)),
    ("children", ctypes.POINTER(ctypes.POINTER(ArrowArray))),
    ("dictionary", ctypes.POINTER(ArrowArray)),
    ("release", ctypes.CFUNCTYPE(None, ctypes.POINTER(ArrowArray))),
    ("private_data", ctypes.c_void_p),
]


# ── Helpers: build Arrow arrays from Python ──────────────────────

def bitmap_bytes(valid_bits: list[bool]) -> bytes:
    """Pack a list of bools into a validity bitmap."""
    n = len(valid_bits)
    nbytes = (n + 7) // 8
    bm = bytearray(nbytes)
    for i, v in enumerate(valid_bits):
        if v:
            bm[i // 8] |= 1 << (i % 8)
    return bytes(bm)


def make_float64_column(values: list[float | None]) -> tuple[ArrowSchema, ArrowArray]:
    """Build an Arrow float64 column from Python values."""
    n = len(values)
    valid = [v is not None for v in values]
    doubles = [v if v is not None else 0.0 for v in values]

    # Allocate buffers
    bm = bitmap_bytes(valid)
    bm_buf = (ctypes.c_uint8 * len(bm))(*bm)
    data_buf = (ctypes.c_double * n)(*doubles)

    buffers = (ctypes.c_void_p * 2)(
        ctypes.cast(bm_buf, ctypes.c_void_p),
        ctypes.cast(data_buf, ctypes.c_void_p),
    )

    schema = ArrowSchema()
    schema.format = b"g"
    schema.name = b""
    schema.metadata = None
    schema.flags = 2  # ARROW_FLAG_NULLABLE
    schema.n_children = 0
    schema.children = None
    schema.dictionary = None
    schema.release = ctypes.CFUNCTYPE(None, ctypes.POINTER(ArrowSchema))(lambda _: None)
    schema.private_data = None

    array = ArrowArray()
    array.length = n
    array.null_count = sum(1 for v in valid if not v)
    array.offset = 0
    array.n_buffers = 2
    array.n_children = 0
    array.buffers = buffers
    array.children = None
    array.dictionary = None
    array.release = ctypes.CFUNCTYPE(None, ctypes.POINTER(ArrowArray))(lambda _: None)
    array.private_data = None

    # Keep references alive
    schema._refs = (bm_buf, data_buf, buffers)
    array._refs = schema._refs

    return schema, array


# ── Tests ────────────────────────────────────────────────────────

def test_pyarrow_roundtrip():
    """Create a pyarrow table, export via C Data Interface, reimport."""
    table = pa.table({
        "price": [100.5, 101.0, None, 99.5, 102.0],
        "volume": [1000.0, 1500.0, 2000.0, None, 1800.0],
    })

    # Export to C Data Interface structs
    schema = ArrowSchema()
    array = ArrowArray()
    schema_ptr = ctypes.byref(schema)
    array_ptr = ctypes.byref(array)

    table.to_batches()[0]._export_to_c(
        ctypes.addressof(array),
        ctypes.addressof(schema),
    )

    # Verify struct schema
    assert schema.format == b"+s"
    assert schema.n_children == 2

    # Verify columns
    assert array.length == 5
    assert array.n_children == 2

    # Re-import back into pyarrow
    batch = pa.RecordBatch._import_from_c(
        ctypes.addressof(array),
        ctypes.addressof(schema),
    )
    result = pa.Table.from_batches([batch])

    assert result.num_columns == 2
    assert result.num_rows == 5
    assert result.column_names == ["price", "volume"]
    assert result.column("price")[0].as_py() == 100.5
    assert result.column("price")[2].as_py() is None


def test_pyarrow_to_polars_zero_copy():
    """Arrow table → Polars DataFrame (zero-copy via Arrow)."""
    table = pa.table({
        "A": [1.0, 2.0, 3.0],
        "B": [4.0, 5.0, 6.0],
    })
    df = pl.from_arrow(table)
    assert df.shape == (3, 2)
    assert df["A"].to_list() == [1.0, 2.0, 3.0]
    assert df["B"].sum() == 15.0


def test_pyarrow_to_pandas():
    """Arrow table → pandas DataFrame."""
    table = pa.table({
        "x": [10.0, None, 30.0],
        "y": [1.0, 2.0, 3.0],
    })
    df = table.to_pandas()
    assert len(df) == 3
    assert list(df.columns) == ["x", "y"]
    assert np.isnan(df["x"].iloc[1])
    assert df["y"].sum() == 6.0


def test_polars_to_arrow_to_pandas():
    """Polars → Arrow → pandas full pipeline."""
    df_pl = pl.DataFrame({
        "col1": [1.0, 2.0, 3.0, 4.0, 5.0],
        "col2": [10.0, 20.0, 30.0, 40.0, 50.0],
    })
    table = df_pl.to_arrow()
    df_pd = table.to_pandas()
    assert df_pd["col1"].sum() == 15.0
    assert df_pd["col2"].mean() == 30.0


def test_null_handling_roundtrip():
    """Verify nulls survive Arrow round-trip."""
    original = pa.table({
        "vals": pa.array([1.0, None, 3.0, None, 5.0], type=pa.float64()),
    })

    # pyarrow → polars → back to arrow → pandas
    df_pl = pl.from_arrow(original)
    table_back = df_pl.to_arrow()
    df_pd = table_back.to_pandas()

    assert df_pd["vals"].iloc[0] == 1.0
    assert np.isnan(df_pd["vals"].iloc[1])
    assert df_pd["vals"].iloc[2] == 3.0
    assert np.isnan(df_pd["vals"].iloc[3])
    assert df_pd["vals"].iloc[4] == 5.0
