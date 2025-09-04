#pragma once
#include <vector>
#include <cmath>
#include <optional>

namespace vec {
inline std::optional<double> l2_distance(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size()!=b.size()) return std::nullopt;
    double s=0.0;
    for (size_t i=0;i<a.size();++i){ double d=a[i]-b[i]; s+=d*d; }
    return std::sqrt(s);
}
} // namespace vec
