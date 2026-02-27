#pragma once
#include <cmath>
#include <cstdio>

constexpr float kOrbPi = 3.14159265358979323846f;

struct OrbitalInfo {
    int   n, l, m;
    const char* name;         // e.g. "1s", "2p_z"
    const char* full_label;   // e.g. "1s (n=1 l=0 m=0)"
    float radial_norm;        // N_nl
    float angular_norm;       // spherical harmonic normalization
    float bounding_radius;    // r_max in Bohr radii
};

// Factorial for small integers
inline double factorial(int n) {
    double r = 1.0;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

// Compute radial normalization constant N_nl
// N_nl = sqrt((2/n)^3 * (n-l-1)! / (2n * ((n+l)!)^3))
// Note: the spec has a simplified form. The standard form is:
// N_nl = sqrt((2/n)^3 * (n-l-1)! / (2n * (n+l)!))
// which is the correct hydrogen radial normalization.
inline float compute_radial_norm(int n, int l) {
    double two_over_n = 2.0 / n;
    double num = factorial(n - l - 1);
    double den = 2.0 * n * factorial(n + l);
    return static_cast<float>(std::sqrt(two_over_n * two_over_n * two_over_n * num / den));
}

// Compute real spherical harmonic normalization constants
// Y_l^m normalization (real form):
// For m=0:  sqrt((2l+1)/(4*pi))
// For m!=0: sqrt(2) * sqrt((2l+1)/(4*pi) * (l-|m|)!/(l+|m|)!)
inline float compute_angular_norm(int l, int m) {
    int am = std::abs(m);
    double base = (2.0 * l + 1.0) / (4.0 * kOrbPi);
    double ratio = factorial(l - am) / factorial(l + am);
    double norm = std::sqrt(base * ratio);
    if (m != 0) norm *= std::sqrt(2.0);
    return static_cast<float>(norm);
}

// Bounding radius: roughly proportional to n^2, tuned so density is negligible beyond
inline float compute_bounding_radius(int n) {
    // Empirical fit: good enough for visual purposes
    switch (n) {
        case 1: return 8.0f;
        case 2: return 20.0f;
        case 3: return 38.0f;
        case 4: return 60.0f;
        default: return static_cast<float>(n * n * 4);
    }
}

// Build the full catalog of 30 orbitals (n=1..4)
struct OrbitalCatalog {
    static constexpr int kMaxOrbitals = 30;
    OrbitalInfo orbitals[kMaxOrbitals];
    int count = 0;

    // Spectroscopic subshell letters
    static char subshell_letter(int l) {
        constexpr char letters[] = "spdf";
        return (l < 4) ? letters[l] : '?';
    }

    // Name strings (stored as static buffers)
    char name_bufs[kMaxOrbitals][32];
    char label_bufs[kMaxOrbitals][64];

    // m-value names for display
    static const char* m_suffix(int l, int m) {
        if (l == 0) return "";
        if (l == 1) {
            if (m == -1) return "_y";
            if (m ==  0) return "_z";
            if (m ==  1) return "_x";
        }
        if (l == 2) {
            if (m == -2) return "_xy";
            if (m == -1) return "_yz";
            if (m ==  0) return "_z2";
            if (m ==  1) return "_xz";
            if (m ==  2) return "_x2-y2";
        }
        if (l == 3) {
            if (m == -3) return "_y(3x2-y2)";
            if (m == -2) return "_xyz";
            if (m == -1) return "_yz2";
            if (m ==  0) return "_z3";
            if (m ==  1) return "_xz2";
            if (m ==  2) return "_z(x2-y2)";
            if (m ==  3) return "_x(x2-3y2)";
        }
        return "";
    }

    void build() {
        count = 0;
        for (int n = 1; n <= 4; ++n) {
            for (int l = 0; l < n; ++l) {
                for (int m = -l; m <= l; ++m) {
                    auto& o = orbitals[count];
                    o.n = n;
                    o.l = l;
                    o.m = m;
                    o.radial_norm    = compute_radial_norm(n, l);
                    o.angular_norm   = compute_angular_norm(l, m);
                    o.bounding_radius = compute_bounding_radius(n);

                    std::snprintf(name_bufs[count], sizeof(name_bufs[count]),
                                  "%d%c%s", n, subshell_letter(l), m_suffix(l, m));
                    o.name = name_bufs[count];

                    std::snprintf(label_bufs[count], sizeof(label_bufs[count]),
                                  "%d%c%s (n=%d l=%d m=%d)",
                                  n, subshell_letter(l), m_suffix(l, m), n, l, m);
                    o.full_label = label_bufs[count];

                    ++count;
                }
            }
        }
    }
};
