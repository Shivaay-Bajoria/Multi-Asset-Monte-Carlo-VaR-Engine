# Multi-Asset Monte Carlo VaR Engine

A C++ engine that simulates thousands of correlated future market scenarios for a multi-asset portfolio and computes **Value at Risk (VaR)** and **Conditional VaR / Expected Shortfall (CVaR)** — the two core risk metrics used by trading desks, risk managers, and regulators to answer one question:

> *"How much could we realistically lose, and how bad does it get when things go wrong?"*

Built with modern C++ (C++17/20), Eigen for linear algebra, and multi-threaded Monte Carlo simulation.

---

## Table of Contents

- [Overview](#overview)
- [The Financial Mathematics](#the-financial-mathematics)
  - [1. Geometric Brownian Motion](#1-geometric-brownian-motion)
  - [2. Correlating Assets with Cholesky Decomposition](#2-correlating-assets-with-cholesky-decomposition)
  - [3. VaR and CVaR](#3-var-and-cvar)
  - [4. Design Refinements](#4-design-refinements)
- [Architecture](#architecture)
  - [Pipeline](#pipeline)
  - [Core Classes](#core-classes)
- [Tech Stack](#tech-stack)
- [Performance: Parallelizing the Simulation](#performance-parallelizing-the-simulation)
- [Getting Started](#getting-started)
- [Roadmap / Stretch Goals](#roadmap--stretch-goals)
- [References](#references)

---

## Overview

Given a portfolio of correlated assets (e.g. equities), the engine:

1. Estimates each asset's drift ($\mu$), volatility ($\sigma$), and the covariance structure between assets from historical price data.
2. Simulates **N** correlated future price paths using Geometric Brownian Motion (GBM).
3. Revalues the portfolio under each simulated scenario to build a distribution of possible Profit & Loss (P&L) outcomes.
4. Computes **VaR** and **CVaR** from that distribution, along with a confidence interval on the VaR estimate itself.

This mirrors — at a smaller scale — how real risk systems on trading desks work.

---

## The Financial Mathematics

### 1. Geometric Brownian Motion

Asset prices are modeled as following a random walk with drift and volatility. Critically, we simulate **log-returns**, not price levels directly — this keeps prices strictly positive and matches how GBM is derived from Itô calculus:

$$
S_{t+1} = S_t \cdot \exp\left[\left(\mu - \frac{\sigma^2}{2}\right)\Delta t + \sigma\sqrt{\Delta t}\,Z\right]
$$

Where:

| Symbol | Meaning |
|---|---|
| $S_t$ | Asset price at time $t$ |
| $\mu$ | Drift (expected return) |
| $\sigma$ | Historical volatility |
| $\Delta t$ | Time step (e.g. $1/252$ for one trading day) |
| $Z$ | A draw from a Standard Normal distribution, $Z \sim N(0,1)$ |

**Why the $-\frac{\sigma^2}{2}$ term matters:** this is the *Itô correction*. Naively simulating $S_t(1 + \mu \Delta t + \sigma\sqrt{\Delta t}Z)$ or dropping this term introduces an upward bias in simulated prices — a subtle but common bug that will noticeably skew your VaR estimate. Using the log-return / exponential form avoids it entirely.

**On the drift term $\mu$:** for short-horizon risk measurement (1-day, 10-day VaR), historical mean return is a very noisy estimator — it can dominate and distort results in ways that have little to do with actual risk. Many production VaR systems instead set $\mu = 0$ or $\mu = r_f$ (risk-free rate) for a risk-neutral simulation, since VaR is primarily about **volatility and tail behavior**, not directional drift. This engine defaults to zero drift, with historical drift available as a configurable option.

### 2. Correlating Assets with Cholesky Decomposition

Assets don't move independently — tech stocks tend to move together, for example. To reflect this, we don't generate independent random shocks per asset; we generate **correlated** ones.

**Step 1 — Covariance matrix.** From historical returns, compute the covariance matrix $\Sigma$ across all $n$ assets.

**Step 2 — Cholesky decomposition.** Decompose $\Sigma$ into a lower triangular matrix $L$ such that:

$$
\Sigma = L L^T
$$

**Step 3 — Correlate the shocks.** Draw a vector of $n$ *independent* standard normal variables $Z = (Z_1, ..., Z_n)$, then transform:

$$
Z_{\text{correlated}} = L Z
$$

The resulting vector has the same covariance structure as the historical data, so simulated assets rise and fall together the way they actually do in the market.

> **Caveat worth knowing:** a multivariate normal + Cholesky approach assumes correlations are constant. In reality, correlations tend to spike during market crashes — the exact moment VaR matters most. This is a well-known limitation of the "vanilla" approach, addressed in the [Roadmap](#roadmap--stretch-goals) below.

### 3. VaR and CVaR

After simulating $N$ scenarios (e.g. $N = 100{,}000$), each scenario produces one portfolio P&L outcome. Sort all $N$ P&L values from worst to best:

- **99% VaR** — the P&L at the 1st percentile of the sorted distribution. *"We are 99% confident losses will not exceed this amount."*
- **99% CVaR (Expected Shortfall)** — the **average** of all P&L outcomes worse than the VaR threshold. *"If we do land in that worst 1%, this is what we should expect to actually lose."*

CVaR is increasingly preferred over VaR alone because it's a *coherent* risk measure (it satisfies subadditivity — diversification never makes your risk measure worse) and it captures tail severity, not just a single cutoff point.

### 4. Design Refinements

Beyond the base spec, this engine incorporates a few additions that reflect how real risk systems are validated:

- **Confidence interval on the VaR estimate itself.** A VaR computed from 100,000 simulated paths is still a *statistical estimate* with sampling noise. The engine bootstraps the simulated P&L distribution to report a confidence interval around the VaR number, not just a single point estimate.
- **Instrument/Position abstraction.** Even though v1 only prices linear equity positions, the codebase separates `Instrument` pricing logic from portfolio aggregation, so options or fixed income instruments can be added later without restructuring.

---

## Architecture

### Pipeline

```
Historical Prices (CSV)
        │
        ▼
   MarketData          → computes returns, μ, σ, covariance matrix Σ
        │
        ▼
   Portfolio            → holds positions, computes Cholesky matrix L
        │
        ▼
 MonteCarloEngine       → generates correlated GBM paths (multi-threaded)
        │
        ▼
   RiskMetrics           → sorts P&L, computes VaR, CVaR, confidence interval
```

### Core Classes

| Class | Responsibility |
|---|---|
| `MarketData` | Reads historical price CSVs, computes daily returns, $\mu$, $\sigma$ per asset |
| `Portfolio` | Holds positions/weights, computes covariance matrix $\Sigma$ and Cholesky factor $L$ |
| `Instrument` / `Position` | Abstraction for individual holdings — enables future extension beyond linear equity exposure |
| `MonteCarloEngine` | Owns RNG setup, runs the GBM simulation loop across threads, produces the P&L array |
| `RiskMetrics` | Sorts simulated P&L, computes VaR, CVaR, and bootstrap confidence intervals |

---

## Tech Stack

| Component | Choice | Why |
|---|---|---|
| Language | C++17 / C++20 | Performance, control over memory layout for large simulation arrays |
| Linear Algebra | [Eigen](https://eigen.tuxfamily.org/) | Industry-standard C++ template library; makes Cholesky decomposition and matrix ops trivial and fast |
| Randomness | `<random>` — `std::mt19937` + `std::normal_distribution` | Mersenne Twister is a high-quality, well-tested PRNG; avoids the pitfalls of `rand()` |
| Parallelism | `std::thread` / `std::async` / `std::execution::par` | Monte Carlo simulations are embarrassingly parallel — each path is independent |
| Build System | CMake | Standard, cross-platform, plays well with Eigen |

---

## Performance: Parallelizing the Simulation

Running a large number of simulations (e.g. 1,000,000 paths) on a single core can take several seconds. Because each Monte Carlo path is statistically independent of every other, the workload is **embarrassingly parallel** — it splits cleanly across cores with no synchronization needed during the simulation itself (only when aggregating results at the end).

This engine splits the total path count evenly across all available logical cores, with each thread running its own independent RNG stream (seeded distinctly to avoid correlated randomness across threads) and writing into a pre-allocated shared results buffer at disjoint indices — avoiding lock contention entirely.

---

## Getting Started

```bash
# Clone and build
git clone <your-repo-url>
cd multi-asset-mc-var-engine
mkdir build && cd build
cmake ..
make

# Run
./var_engine --data ../data/prices.csv --paths 100000 --confidence 0.99
```

*(Update this section once the CLI / config format is finalized.)*

---

## Roadmap / Stretch Goals

- [ ] **Student-t copula** for dependence structure — captures the tendency of correlations to spike during market stress, which Gaussian + Cholesky misses entirely
- [ ] **GARCH(1,1) volatility filtering** — model volatility clustering instead of assuming constant $\sigma$
- [ ] **Historical / filtered-historical simulation mode** as an alternative to pure GBM, for comparison
- [ ] **Backtesting module** — Kupiec and Christoffersen tests to validate VaR exception rates against realized P&L
- [ ] **Options/derivatives support** — extend `Instrument` hierarchy for nonlinear payoffs
- [ ] **GPU acceleration** for very large path counts

---

## References

- Hull, J. *Options, Futures, and Other Derivatives* — GBM and risk-neutral pricing
- Jorion, P. *Value at Risk: The New Benchmark for Managing Financial Risk*
- Artzner, Delbaen, Eber, Heath — *Coherent Measures of Risk* (the paper motivating CVaR over VaR)
- Basel Committee on Banking Supervision — backtesting frameworks (Kupiec, Christoffersen)
