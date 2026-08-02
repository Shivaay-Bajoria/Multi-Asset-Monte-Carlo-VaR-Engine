#include "MarketData.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <cmath>

MarketData::MarketData(const std::string& csvPath) {
    loadCSV(csvPath);
    computeLogReturns();
    computeStatistics();
}

void MarketData::loadCSV(const std::string& csvPath) {
    std::ifstream file(csvPath);
    if (!file.is_open()) {
        throw std::runtime_error("MarketData: could not open file: " + csvPath);
    }

    std::vector<double> priceValues;
    std::string line;

    // First line is the header (Date,Close) -- skip it.
    if (!std::getline(file, line)) {
        throw std::runtime_error("MarketData: file is empty: " + csvPath);
    }

    int lineNumber = 1;
    while (std::getline(file, line)) {
        ++lineNumber;

        // Strip a trailing '\r' if present (files with Windows-style
        // CRLF line endings, e.g. written by Python's csv module,
        // otherwise leave a stray '\r' stuck to the last field).
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue; // skip blank lines
        }

        std::stringstream ss(line);
        std::string dateStr, closeStr;

        if (!std::getline(ss, dateStr, ',') || !std::getline(ss, closeStr, ',')) {
            throw std::runtime_error(
                "MarketData: malformed row at line " + std::to_string(lineNumber) +
                " in " + csvPath);
        }

        double closePrice;
        try {
            closePrice = std::stod(closeStr);
        } catch (const std::exception&) {
            throw std::runtime_error(
                "MarketData: could not parse price '" + closeStr +
                "' at line " + std::to_string(lineNumber));
        }

        if (closePrice <= 0.0) {
            throw std::runtime_error(
                "MarketData: non-positive price at line " + std::to_string(lineNumber) +
                " -- prices must be > 0 to take a log return.");
        }

        priceValues.push_back(closePrice);
    }

    if (priceValues.size() < 2) {
        throw std::runtime_error(
            "MarketData: need at least 2 price observations to compute a return, got " +
            std::to_string(priceValues.size()));
    }

    // Copy into an Eigen vector.
    prices_.resize(static_cast<Eigen::Index>(priceValues.size()));
    for (size_t i = 0; i < priceValues.size(); ++i) {
        prices_[static_cast<Eigen::Index>(i)] = priceValues[i];
    }
}

void MarketData::computeLogReturns() {
    const Eigen::Index n = prices_.size();
    logReturns_.resize(n - 1);

    for (Eigen::Index t = 0; t < n - 1; ++t) {
        logReturns_[t] = std::log(prices_[t + 1] / prices_[t]);
    }
}

void MarketData::computeStatistics() {
    const Eigen::Index n = logReturns_.size();

    // Mean (daily mu)
    dailyMu_ = logReturns_.mean();

    // Sample standard deviation (ddof = 1, i.e. divide by N-1, not N).
    // We deliberately use N-1 here to match standard statistical practice
    // (and our Python ground-truth script, which uses np.std(..., ddof=1)) --
    // using N would produce a biased (slightly too small) estimate of
    // the true variance.
    if (n < 2) {
        throw std::runtime_error(
            "MarketData: need at least 2 log returns to compute sample std dev.");
    }

    double sumSquaredDeviations = 0.0;
    for (Eigen::Index t = 0; t < n; ++t) {
        double deviation = logReturns_[t] - dailyMu_;
        sumSquaredDeviations += deviation * deviation;
    }

    double sampleVariance = sumSquaredDeviations / static_cast<double>(n - 1);
    dailySigma_ = std::sqrt(sampleVariance);
}

double MarketData::getAnnualizedMu() const {
    return dailyMu_ * TRADING_DAYS_PER_YEAR;
}

double MarketData::getAnnualizedSigma() const {
    return dailySigma_ * std::sqrt(static_cast<double>(TRADING_DAYS_PER_YEAR));
}
