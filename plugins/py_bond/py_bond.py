"""
Bond pricing plugin for CellWright (Python version).

Mirrors the C++ BondPlugin: create bond handles, query price/ytm/coupon/duration.
"""

import math

functions = {
    "pybond":         {"min_args": 4, "max_args": 4},
    "pybondPrice":    {"min_args": 1, "max_args": 1},
    "pybondYtm":      {"min_args": 1, "max_args": 1},
    "pybondCoupon":   {"min_args": 1, "max_args": 1},
    "pybondDuration": {"min_args": 1, "max_args": 1},
}

_store = {}
_counter = 0
FACE = 100.0


def _bond_price(coupon, ytm, maturity, frequency):
    c = coupon * FACE / frequency
    y = ytm / frequency
    n = int(maturity * frequency)
    pv = sum(c / (1.0 + y) ** i for i in range(1, n + 1))
    pv += FACE / (1.0 + y) ** n
    return pv


def _bond_duration(coupon, ytm, maturity, frequency):
    c = coupon * FACE / frequency
    y = ytm / frequency
    n = int(maturity * frequency)
    p = _bond_price(coupon, ytm, maturity, frequency)
    if p == 0.0:
        return 0.0
    weighted = sum((i / frequency) * c / (1.0 + y) ** i for i in range(1, n + 1))
    weighted += (n / frequency) * FACE / (1.0 + y) ** n
    return weighted / p


def pybond(coupon, ytm, maturity, frequency):
    global _counter
    _counter += 1
    handle = f"PyBond#{_counter}"
    _store[handle] = {
        "coupon": float(coupon),
        "ytm": float(ytm),
        "maturity": float(maturity),
        "frequency": float(frequency),
    }
    return handle


def _get_bond(handle):
    if not isinstance(handle, str) or handle not in _store:
        raise KeyError(f"Unknown bond handle: {handle}")
    return _store[handle]


def pybondPrice(handle):
    b = _get_bond(handle)
    return _bond_price(b["coupon"], b["ytm"], b["maturity"], b["frequency"])


def pybondYtm(handle):
    b = _get_bond(handle)
    return b["ytm"]


def pybondCoupon(handle):
    b = _get_bond(handle)
    return b["coupon"]


def pybondDuration(handle):
    b = _get_bond(handle)
    return _bond_duration(b["coupon"], b["ytm"], b["maturity"], b["frequency"])
