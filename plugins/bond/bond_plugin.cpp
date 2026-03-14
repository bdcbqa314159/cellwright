#include "bond_plugin.hpp"
#include "Bond.hpp"
#include <unordered_map>
#include <string>

namespace magic::plugins {

static constexpr size_t MAX_STORE_SIZE = 1000;

static std::unordered_map<std::string, Bond>& store() {
    static std::unordered_map<std::string, Bond> s;
    return s;
}

static int& counter() {
    static int c = 0;
    return c;
}

CellValue BondPlugin::call(const std::string& func_name, const std::vector<CellValue>& args) const {
    if (func_name == "bond") {
        double coupon    = to_double(args[0]);
        double ytm       = to_double(args[1]);
        double maturity  = to_double(args[2]);
        double frequency = to_double(args[3]);

        // Prevent unbounded growth of the handle store
        if (store().size() >= MAX_STORE_SIZE)
            store().clear();

        std::string handle = "Bond#" + std::to_string(++counter());
        store()[handle] = Bond{coupon, ytm, maturity, frequency};
        return CellValue{handle};
    }

    if (func_name == "bondPrice" || func_name == "bondYtm" ||
        func_name == "bondCoupon" || func_name == "bondDuration") {
        if (!is_string(args[0])) return CellValue{CellError::VALUE};
        const auto& handle = as_string(args[0]);
        auto it = store().find(handle);
        if (it == store().end()) return CellValue{CellError::NAME};
        const Bond& b = it->second;

        if (func_name == "bondPrice")    return CellValue{b.price()};
        if (func_name == "bondYtm")      return CellValue{b.ytm};
        if (func_name == "bondCoupon")   return CellValue{b.coupon};
        if (func_name == "bondDuration") return CellValue{b.duration()};
    }

    return CellValue{CellError::NAME};
}

}  // namespace magic::plugins
