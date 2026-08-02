"""
Generates a synthetic single-asset price series using GBM with KNOWN
parameters, so we have ground truth to validate the C++ MarketData class
against.

This is deliberately independent of the C++ code -- it's the "answer key,"
not a stand-in for it. If MarketData's computed mu/sigma don't match what
this script reports, we know there's a bug in the C++ (or in this script --
but at least the two are computed two different ways, which is the point).
"""

import numpy as np
import csv
from datetime import date, timedelta

# --- Known "true" parameters (annualized) ---
TRUE_MU_ANNUAL = 0.10      # 10% expected annual drift
TRUE_SIGMA_ANNUAL = 0.25   # 25% annual volatility
S0 = 100.0                 # starting price
N_DAYS = 500                # number of trading days to simulate
TRADING_DAYS_PER_YEAR = 252
DT = 1.0 / TRADING_DAYS_PER_YEAR

SEED = 42
rng = np.random.default_rng(SEED)

# Simulate using the SAME formula as our C++ engine will eventually use
# for forward simulation: S_{t+1} = S_t * exp((mu - sigma^2/2)*dt + sigma*sqrt(dt)*Z)
prices = np.zeros(N_DAYS + 1)
prices[0] = S0
Z = rng.standard_normal(N_DAYS)

for t in range(N_DAYS):
    drift = (TRUE_MU_ANNUAL - 0.5 * TRUE_SIGMA_ANNUAL**2) * DT
    shock = TRUE_SIGMA_ANNUAL * np.sqrt(DT) * Z[t]
    prices[t + 1] = prices[t] * np.exp(drift + shock)

# --- Write CSV: Date,Close ---
start_date = date(2023, 1, 2)
dates = [start_date + timedelta(days=i) for i in range(N_DAYS + 1)]

csv_path = "synthetic_prices.csv"
with open(csv_path, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["Date", "Close"])
    for d, p in zip(dates, prices):
        writer.writerow([d.isoformat(), f"{p:.4f}"])

# --- Compute ground-truth statistics DIRECTLY from the generated series ---
# (This is what MarketData should recover -- the SAMPLE stats, not the
# theoretical TRUE_MU_ANNUAL/TRUE_SIGMA_ANNUAL, since any finite sample
# will deviate from the population parameters.)
log_returns = np.diff(np.log(prices))
sample_daily_mu = np.mean(log_returns)
sample_daily_sigma = np.std(log_returns, ddof=1)  # sample std (N-1)

print("=== Synthetic Data Generation Report ===")
print(f"Seed: {SEED}")
print(f"N_DAYS: {N_DAYS}")
print(f"True annual mu (input):    {TRUE_MU_ANNUAL}")
print(f"True annual sigma (input): {TRUE_SIGMA_ANNUAL}")
print()
print("--- Ground truth stats computed from the generated CSV (what C++ should match) ---")
print(f"Sample daily mu (mean log return):     {sample_daily_mu:.8f}")
print(f"Sample daily sigma (std of log return): {sample_daily_sigma:.8f}")
print(f"Annualized mu (sample_daily_mu * 252):        {sample_daily_mu * TRADING_DAYS_PER_YEAR:.6f}")
print(f"Annualized sigma (sample_daily_sigma * sqrt(252)): {sample_daily_sigma * np.sqrt(TRADING_DAYS_PER_YEAR):.6f}")
print()
print(f"CSV written to: {csv_path}")
print(f"First 3 rows:")
for d, p in list(zip(dates, prices))[:3]:
    print(f"  {d.isoformat()},{p:.4f}")
print(f"Last row: {dates[-1].isoformat()},{prices[-1]:.4f}")
