// Multi-Asset Monte Carlo VaR Engine
//
// Single entry point for the project. As we build out each step
// (Portfolio, MonteCarloEngine, RiskMetrics), this file grows to wire
// them together -- it stays the one place you run to see the whole
// pipeline in action.
//
// Progress so far:
//   [x] Step 1 -- Eigen + build sanity check (Cholesky smoke test)
//   [x] Step 2 -- MarketData: load historical prices, compute mu/sigma
//   [ ] Step 3 -- single-asset GBM simulation
//   [ ] Step 4 -- RiskMetrics (VaR/CVaR) on single-asset output
//   [ ] Step 5 -- multi-asset covariance + Cholesky
//   [ ] Step 6 -- correlated multi-asset simulation
//   [ ] Step 7 -- parallelization
//
// Run with: ./var_engine <path-to-price-csv>
// Defaults to ../data/prices.csv if no argument is given.

#include <iostream>
#include <iomanip>
#include <Eigen/Dense>
#include "MarketData.h"

namespace {

// Kept from Step 1 as a standalone sanity check you can call any time you
// touch the build system or Eigen setup -- not part of the main pipeline.
void runEigenSmokeTest() {
    std::cout << "--- Eigen / Cholesky smoke test ---\n";

    Eigen::Matrix3d covariance;
    covariance << 0.04, 0.01, 0.00,
                  0.01, 0.09, 0.02,
                  0.00, 0.02, 0.16;

    Eigen::LLT<Eigen::Matrix3d> cholesky(covariance);
    if (cholesky.info() != Eigen::Success) {
        std::cerr << "  Cholesky decomposition failed.\n";
        return;
    }

    Eigen::Matrix3d L = cholesky.matrixL();
    Eigen::Matrix3d reconstructed = L * L.transpose();
    double maxError = (reconstructed - covariance).cwiseAbs().maxCoeff();

    std::cout << "  Max reconstruction error: " << maxError
              << (maxError < 1e-10 ? "  [OK]\n" : "  [FAILED]\n");
    std::cout << "\n";
}

void runMarketDataStep(const std::string& csvPath) {
    std::cout << "--- MarketData: " << csvPath << " ---\n";

    MarketData data(csvPath);

    std::cout << std::fixed << std::setprecision(8);
    std::cout << "  Observations: " << data.numObservations()
              << "   Log returns: " << data.getLogReturns().size() << "\n";
    std::cout << "  Daily mu:    " << data.getDailyMu() << "\n";
    std::cout << "  Daily sigma: " << data.getDailySigma() << "\n";

    std::cout << std::setprecision(6);
    std::cout << "  Annualized mu:    " << data.getAnnualizedMu() << "\n";
    std::cout << "  Annualized sigma: " << data.getAnnualizedSigma() << "\n";
    std::cout << "  First price: " << data.getPrices()[0]
              << "   Last price: " << data.getPrices()[data.getPrices().size() - 1] << "\n";
    std::cout << "\n";
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "=== Multi-Asset Monte Carlo VaR Engine ===\n\n";

    runEigenSmokeTest();

    std::string csvPath = (argc > 1) ? argv[1] : "../data/prices.csv";

    try {
        runMarketDataStep(csvPath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
