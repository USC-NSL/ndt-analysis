#include "util.h"

#include <algorithm>

#include <gsl/gsl_fit.h>
#include <gsl/gsl_histogram2d.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_vector.h>

namespace tcp_util {

bool After(uint32_t first, uint32_t second) {
   return ((first > second && first - second < 0x7FFFFFFF) ||
           (first < second && second - first > 0x7FFFFFFF));
}

bool Before(uint32_t first, uint32_t second) {
    return After(second, first);
}

bool Between(uint32_t middle, uint32_t first, uint32_t second) {
    return Before(first, middle) && After(second, middle);
}

bool RangeIncluded(uint32_t first_start, uint32_t first_end,
        uint32_t second_start, uint32_t second_end) {
    return ((first_start == second_start ||
             Between(first_start, second_start, second_end)) &&
            (first_end == second_end ||
             Between(first_end, second_start, second_end)));
}

bool Overlaps(uint32_t left_a, uint32_t right_a,
        uint32_t left_b, uint32_t right_b) {
    return (!After(left_a, right_b) && !After(left_b, right_a));
}

}  // namespace tcp_util


namespace stats_util {

double PearsonCorrelation(const std::vector<double> x,
        const std::vector<double> y) {
    if (x.empty()) {
        return 0;
    }

    const size_t num_samples = x.size();
    gsl_vector_const_view gsl_x =
        gsl_vector_const_view_array(&x[0], num_samples);
    gsl_vector_const_view gsl_y =
        gsl_vector_const_view_array(&y[0], num_samples);
    
    return gsl_stats_correlation(
            static_cast<double*>(gsl_x.vector.data), 1,
            static_cast<double*>(gsl_y.vector.data), 1,
            num_samples);
}

LinearFitParameters LinearFit(const std::vector<double> x,
        const std::vector<double> y) {
    if (x.empty()) {
        return LinearFitParameters{0, 0, 0, 0, 0, 0};
    }

    const size_t num_samples = x.size();
    gsl_vector_const_view gsl_x =
        gsl_vector_const_view_array(&x[0], num_samples);
    gsl_vector_const_view gsl_y =
        gsl_vector_const_view_array(&y[0], num_samples);
    
    LinearFitParameters fit;
    gsl_fit_linear(
            static_cast<double*>(gsl_x.vector.data), 1,
            static_cast<double*>(gsl_y.vector.data), 1,
            num_samples,
            &fit.c_0, &fit.c_1,
            &fit.cov_00, &fit.cov_01, &fit.cov_11,
            &fit.sum_sq);
    return fit;
}

double LinearFitValueForX(
        const LinearFitParameters& fit, const double x) {
    double y, y_err;
    gsl_fit_linear_est(
            x, fit.c_0, fit.c_1,
            fit.cov_00, fit.cov_01, fit.cov_11,
            &y, &y_err);
    return y;
}

std::vector<std::pair<double, double>> PopulatedHistogramBins(
        std::vector<std::pair<double, double>> samples,
        size_t x_interval, size_t y_interval) {
    // Find the min/max values of the input dimensions to
    // determine the histogram ranges
    std::vector<double> firsts, seconds;
    vector_util::SplitPairs(samples, &firsts, &seconds);
    auto x_minmax = std::minmax_element(firsts.begin(), firsts.end());
    auto y_minmax = std::minmax_element(seconds.begin(), seconds.end());

    // Generate the histogram with uniformly distributed bins
    const double x_range = *x_minmax.second - *x_minmax.first;
    const double y_range = *y_minmax.second - *y_minmax.first;
    const size_t num_xbins = std::max((int) (x_range / x_interval), 1);
    const size_t num_ybins = std::max((int) (y_range / y_interval), 1);
    gsl_histogram2d* histogram =
        gsl_histogram2d_alloc(num_xbins, num_ybins);
    gsl_histogram2d_set_ranges_uniform(
            histogram,
            *x_minmax.first - 1, *x_minmax.second + 1,
            *y_minmax.first - 1, *y_minmax.second + 1);

    // Add all samples
    for (auto sample : samples) {
        gsl_histogram2d_increment(histogram, sample.first, sample.second);
    }
    
    // Find and return the centers of all bins with at least
    // one value
    std::vector<std::pair<double, double>> populated_bins;
    for (size_t i = 0; i < num_xbins; i++) {
        for (size_t j = 0; j < num_ybins; j++) {
            if (gsl_histogram2d_get(histogram, i, j) > 0) {
                // Get the center for the current bin
                double x_lower, x_upper, y_lower, y_upper;
                gsl_histogram2d_get_xrange(histogram, i, &x_lower, &x_upper);
                gsl_histogram2d_get_yrange(histogram, j, &y_lower, &y_upper);
                const double x_center = (x_lower + x_upper) / 2;
                const double y_center = (y_lower + y_upper) / 2;
                populated_bins.push_back(
                        std::make_pair(x_center, y_center));
            }
        }
    }
    gsl_histogram2d_free(histogram);

    return populated_bins;
}

}  // namespace stats_util
