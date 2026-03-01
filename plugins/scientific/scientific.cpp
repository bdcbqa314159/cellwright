#include "scientific.hpp"
#include <PluginFactory.hpp>
#include <cmath>
#include <string>

namespace magic::plugins {

// Physical constants
static constexpr double c   = 299792458.0;         // speed of light (m/s)
static constexpr double h   = 6.62607015e-34;       // Planck constant (J·s)
static constexpr double k_B = 1.380649e-23;         // Boltzmann constant (J/K)
static constexpr double R   = 8.314462618;           // gas constant (J/(mol·K))

CellValue Scientific::call(const std::string& func_name,
                           const std::vector<CellValue>& args) const {

    // ── sigmoid: 1/(1 + e^(-x)) ────────────────────────────────────────────
    if (func_name == "sigmoid") {
        double x = to_double(args[0]);
        return CellValue{1.0 / (1.0 + std::exp(-x))};
    }

    // ── logit: ln(p/(1-p)), domain (0,1) exclusive ─────────────────────────
    if (func_name == "logit") {
        double p = to_double(args[0]);
        if (p <= 0.0 || p >= 1.0) return CellValue{CellError::VALUE};
        return CellValue{std::log(p / (1.0 - p))};
    }

    // ── entropy: -Σ pᵢ·ln(pᵢ)  (Shannon entropy) ──────────────────────────
    if (func_name == "entropy") {
        double sum = 0.0;
        for (const auto& arg : args) {
            double p = to_double(arg);
            if (p < 0.0 || p > 1.0) return CellValue{CellError::VALUE};
            if (p > 0.0) sum -= p * std::log(p);
        }
        return CellValue{sum};
    }

    // ── decay: N₀·e^(-λt) ──────────────────────────────────────────────────
    if (func_name == "decay") {
        double n0     = to_double(args[0]);
        double lambda = to_double(args[1]);
        double t      = to_double(args[2]);
        return CellValue{n0 * std::exp(-lambda * t)};
    }

    // ── halflife: N₀·(½)^(t/t½) ────────────────────────────────────────────
    if (func_name == "halflife") {
        double n0     = to_double(args[0]);
        double t_half = to_double(args[1]);
        double t      = to_double(args[2]);
        if (t_half == 0.0) return CellValue{CellError::DIV0};
        return CellValue{n0 * std::pow(0.5, t / t_half)};
    }

    // ── blackbody: Planck spectral radiance B(λ,T)  (W·sr⁻¹·m⁻³) ──────────
    //    args: (wavelength_nm, temperature_K)
    if (func_name == "blackbody") {
        double lambda_nm = to_double(args[0]);
        double temp_k    = to_double(args[1]);
        if (lambda_nm <= 0.0 || temp_k <= 0.0) return CellValue{CellError::VALUE};

        double lam = lambda_nm * 1.0e-9;  // nm → m
        double lam5 = lam * lam * lam * lam * lam;
        double exponent = h * c / (lam * k_B * temp_k);
        double numerator   = 2.0 * h * c * c;
        double denominator = lam5 * (std::exp(exponent) - 1.0);
        return CellValue{numerator / denominator};
    }

    // ── doppler: f₀·c/(c + v)  (+v = receding) ────────────────────────────
    if (func_name == "doppler") {
        double f0 = to_double(args[0]);
        double v  = to_double(args[1]);
        if (c + v == 0.0) return CellValue{CellError::DIV0};
        return CellValue{f0 * c / (c + v)};
    }

    // ── ideal_gas: solve PV = nRT for the variable named by the 4th arg ────
    //    args: (known1, known2, known3, "P"|"V"|"n"|"T")
    //    The 3 known values are the other variables in alphabetical order.
    if (func_name == "ideal_gas") {
        if (!is_string(args[3])) return CellValue{CellError::VALUE};
        const std::string& solve_for = as_string(args[3]);
        double a = to_double(args[0]);
        double b = to_double(args[1]);
        double c_val = to_double(args[2]);

        if (solve_for == "P") {
            // known: V, n, T → P = nRT/V
            double V = a, n = b, T = c_val;
            if (V == 0.0) return CellValue{CellError::DIV0};
            return CellValue{n * R * T / V};
        }
        if (solve_for == "V") {
            // known: P, n, T → V = nRT/P
            double P = a, n = b, T = c_val;
            if (P == 0.0) return CellValue{CellError::DIV0};
            return CellValue{n * R * T / P};
        }
        if (solve_for == "n") {
            // known: P, V, T → n = PV/(RT)
            double P = a, V = b, T = c_val;
            if (R * T == 0.0) return CellValue{CellError::DIV0};
            return CellValue{P * V / (R * T)};
        }
        if (solve_for == "T") {
            // known: P, V, n → T = PV/(nR)
            double P = a, V = b, n = c_val;
            if (n * R == 0.0) return CellValue{CellError::DIV0};
            return CellValue{P * V / (n * R)};
        }
        return CellValue{CellError::VALUE};  // unknown solve target
    }

    // ── normalize: (x - min)/(max - min) ───────────────────────────────────
    if (func_name == "normalize") {
        double x   = to_double(args[0]);
        double lo  = to_double(args[1]);
        double hi  = to_double(args[2]);
        if (hi == lo) return CellValue{CellError::DIV0};
        return CellValue{(x - lo) / (hi - lo)};
    }

    // ── pearson: Pearson correlation coefficient ────────────────────────────
    //    args: flat paired list x1,y1,x2,y2,...  (must be even, ≥ 4)
    if (func_name == "pearson") {
        int n = static_cast<int>(args.size());
        if (n % 2 != 0) return CellValue{CellError::VALUE};

        int pairs = n / 2;
        double sum_x = 0, sum_y = 0;
        for (int i = 0; i < n; i += 2) {
            sum_x += to_double(args[i]);
            sum_y += to_double(args[i + 1]);
        }
        double mean_x = sum_x / pairs;
        double mean_y = sum_y / pairs;

        double num = 0, den_x = 0, den_y = 0;
        for (int i = 0; i < n; i += 2) {
            double dx = to_double(args[i])     - mean_x;
            double dy = to_double(args[i + 1]) - mean_y;
            num  += dx * dy;
            den_x += dx * dx;
            den_y += dy * dy;
        }
        double den = std::sqrt(den_x * den_y);
        if (den == 0.0) return CellValue{CellError::DIV0};
        return CellValue{num / den};
    }

    return CellValue{CellError::NAME};
}

}  // namespace magic::plugins

REGISTER_PLUGIN(magic::plugins::Scientific)
