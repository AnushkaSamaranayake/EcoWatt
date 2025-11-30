Power Measurement — Milestone 5

Objective
Measure MCU power draw before and after power optimizations and report results.

Recommended hardware (optional)
- USB power meter (inline) OR
- INA219 breakout connected to a host PC running `power_logger.py` (example provided)

Method A — Using INA219 + power_logger.py
1. Wire INA219 in series with NodeMCU USB power supply according to the script's README.
2. Run `power_logger.py` to capture current vs time while device runs baseline firmware.
3. Enable power optimizations (set CPU to 80MHz, enable light-sleep hints) and rerun logger.
4. Compare average current, peak current, and duty-cycle.

Fields to report
- Technique used (CPU scaling, light-sleep, peripheral gating)
- Measurement tool (model)
- Sampling duration (s)
- Number of samples
- Average current (mA)
- Peak current (mA)
- Estimated energy per upload window (mA·s or mAh)
- Notes on measurement uncertainty

If hardware is unavailable
- Provide an estimation based on CPU frequency scaling and documented ESP8266 power draw numbers.
- Clearly document assumptions and show simple calculations.

Example table to fill
- Baseline: CPU 160MHz, no sleep — Avg current X mA
- Optimized: CPU 80MHz + light sleep — Avg current Y mA
- Savings: (X-Y)/X * 100% = Z%

