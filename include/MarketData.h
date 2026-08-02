#pragma once

#include <Eigen/Dense>
#include <string>

// MarketData
//
// Loads a single asset's historical daily closing prices from a CSV file
// (format: Date,Close) and computes the statistics needed for GBM
// simulation: daily log returns, mu (drift), and sigma (volatility).
//
// Design notes:
//  - We store returns as log returns, not simple returns, to stay
//    consistent with the GBM discretization used elsewhere in this engine.
//  - mu_ and sigma_ are DAILY statistics (not annualized) by default,
//    since that's what the simulation's per-step formula needs directly.
//    Annualized helpers are provided for reporting/sanity-checking.
class MarketData {
public:
    // Loads and processes the CSV immediately (constructor does the work,
    // so a successfully constructed MarketData is always ready to use).
    explicit MarketData(const std::string& csvPath);

    // --- Accessors ---
    double getDailyMu() const { return dailyMu_; }
    double getDailySigma() const { return dailySigma_; }

    // Convenience: annualized versions (assuming 252 trading days/year)
    double getAnnualizedMu() const;
    double getAnnualizedSigma() const;

    const Eigen::VectorXd& getPrices() const { return prices_; }
    const Eigen::VectorXd& getLogReturns() const { return logReturns_; }

    size_t numObservations() const { return static_cast<size_t>(prices_.size()); }

private:
    Eigen::VectorXd prices_;
    Eigen::VectorXd logReturns_;
    double dailyMu_ = 0.0;
    double dailySigma_ = 0.0;

    static constexpr int TRADING_DAYS_PER_YEAR = 252;

    void loadCSV(const std::string& csvPath);
    void computeLogReturns();
    void computeStatistics();
};
