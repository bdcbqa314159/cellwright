"""Smoke tests — validate Python environment and key imports."""

import subprocess
import sys
from pathlib import Path

import pyarrow as pa
import polars as pl
import pandas as pd
import numpy as np


ROOT = Path(__file__).resolve().parents[2]
BUILD_BIN = ROOT / "build" / "bin"


def test_pyarrow_version():
    assert int(pa.__version__.split(".")[0]) >= 23


def test_polars_version():
    assert int(pl.__version__.split(".")[0]) >= 1


def test_pandas_version():
    assert int(pd.__version__.split(".")[0]) >= 3


def test_numpy_version():
    assert int(np.__version__.split(".")[0]) >= 2


def test_arrow_array_roundtrip():
    """Basic Arrow array creation and conversion."""
    arr = pa.array([1.0, 2.0, 3.0, None, 5.0])
    assert arr.type == pa.float64()
    assert len(arr) == 5
    assert arr.null_count == 1


def test_arrow_table_to_polars():
    """Arrow table converts to Polars without copy."""
    table = pa.table({"x": [1, 2, 3], "y": [4.0, 5.0, 6.0]})
    df = pl.from_arrow(table)
    assert df.shape == (3, 2)
    assert df["y"].sum() == 15.0


def test_arrow_table_to_pandas():
    """Arrow table converts to pandas."""
    table = pa.table({"a": [10, 20], "b": ["foo", "bar"]})
    df = table.to_pandas()
    assert list(df.columns) == ["a", "b"]
    assert len(df) == 2


def test_cellwright_binary_exists():
    """Check that the cellwright binary was built."""
    binary = BUILD_BIN / "cellwright"
    assert binary.exists(), f"Binary not found at {binary}"


def test_ctest_passes():
    """Run the C++ test suite via ctest."""
    result = subprocess.run(
        ["ctest", "--output-on-failure", "--test-dir", str(ROOT / "build")],
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert result.returncode == 0, f"ctest failed:\n{result.stdout}\n{result.stderr}"
