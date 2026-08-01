// Step 1 smoke test.
//
// This doesn't do anything "real" yet — no MarketData, no simulation.
// The only goal here is to prove that:
//   1. The project compiles with CMake + C++17
//   2. Eigen is correctly included and usable
//   3. A Cholesky decomposition (which we'll need for correlating assets
//      in Step 5-6) actually runs and produces a sane result
//
// Once this builds and runs cleanly, we move on to Step 2: MarketData.

#include <iostream>
#include <Eigen/Dense>

int main() {
    std::cout << "=== Multi-Asset Monte Carlo VaR Engine ===\n";
    std::cout << "Step 1: build + Eigen smoke test\n\n";

    // A toy 3x3 covariance-like matrix (symmetric positive definite),
    // standing in for what MarketData will eventually compute from
    // real historical returns.
    Eigen::Matrix3d covariance;
    covariance << 0.04, 0.01, 0.00,
                  0.01, 0.09, 0.02,
                  0.00, 0.02, 0.16;

    std::cout << "Sample covariance matrix (Sigma):\n" << covariance << "\n\n";

    // Cholesky decomposition: Sigma = L * L^T
    Eigen::LLT<Eigen::Matrix3d> cholesky(covariance);

    if (cholesky.info() != Eigen::Success) {
        std::cerr << "Cholesky decomposition failed — matrix is not positive definite.\n";
        return 1;
    }

    Eigen::Matrix3d L = cholesky.matrixL();
    std::cout << "Cholesky factor (L):\n" << L << "\n\n";

    // Sanity check: L * L^T should reconstruct the original covariance matrix.
    Eigen::Matrix3d reconstructed = L * L.transpose();
    std::cout << "Reconstructed L * L^T (should match Sigma above):\n"
              << reconstructed << "\n\n";

    double maxError = (reconstructed - covariance).cwiseAbs().maxCoeff();
    std::cout << "Max reconstruction error: " << maxError << "\n";

    if (maxError < 1e-10) {
        std::cout << "\nBuild + Eigen check PASSED. Ready for Step 2 (MarketData).\n";
    } else {
        std::cout << "\nSomething is off — reconstruction error too large.\n";
    }

    return 0;
}
