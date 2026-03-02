#pragma once
#include <cmath>

namespace magic::plugins {

struct Bond {
    double coupon;     // annual coupon rate
    double ytm;        // yield to maturity
    double maturity;   // years
    double frequency;  // payments per year (1, 2, or 4)

    static constexpr double face = 100.0;

    double price() const {
        double c = coupon * face / frequency;
        double y = ytm / frequency;
        int n = static_cast<int>(maturity * frequency);
        double pv = 0.0;
        for (int i = 1; i <= n; ++i)
            pv += c / std::pow(1.0 + y, i);
        pv += face / std::pow(1.0 + y, n);
        return pv;
    }

    double duration() const {
        double c = coupon * face / frequency;
        double y = ytm / frequency;
        int n = static_cast<int>(maturity * frequency);
        double p = price();
        if (p == 0.0) return 0.0;
        double weighted = 0.0;
        for (int i = 1; i <= n; ++i)
            weighted += (static_cast<double>(i) / frequency) * c / std::pow(1.0 + y, i);
        weighted += (static_cast<double>(n) / frequency) * face / std::pow(1.0 + y, n);
        return weighted / p;
    }
};

}  // namespace magic::plugins
