Milestone 5 — Live Demo Checklist

This checklist covers the required demonstrations for Milestone 5. Use it during your 10-minute live demo.

1) Preparation
- Device powered and connected to Wi‑Fi
- Serial monitor open at 115200
- `fault_log.txt` file accessible on LittleFS (for post-demo inspection)

2) Data acquisition and buffering
- Show device polling (serial logs showing samples)
- Show buffer count increments
- Trigger a manual upload (or wait for upload interval)
- Verify upload success on cloud side

3) Secure transmission
- Show upload payload size and that encryption/MAC steps completed (if implemented)
- Show cloud ACK and any returned config/commands

4) Remote configuration
- From cloud/API, send a config update (e.g., sampling_interval)
- Show device logs: config staged, applied (after next upload)
- Show EEPROM change persisted (reboot to confirm if desired)

5) Command execution
- Queue a write command on cloud
- Show device forwarding command to Inverter SIM
- Show Inverter SIM response and device reporting result back to cloud

6) FOTA success
- Initiate FOTA with a correct manifest and SHA-256 hash
- Show chunked download progress, verification, controlled reboot, and final boot confirmation

7) FOTA failure & rollback
- Initiate FOTA with bad manifest/hash
- Show that device detects verification failure and rolls back to previous firmware
- Show logs and final boot back to previous version

8) Power optimization comparison
- Show baseline (before changes) current/voltage log or estimate
- Show optimized mode measurement using `power_logger.py` (INA219) or estimation results
- Point out the measurement method and sample logs

9) Fault injection tests
- Trigger network error (e.g., turn off Inverter SIM endpoint) and show retry/backoff and fault logs
- Trigger Inverter SIM malformed frame (use provided fault samples) and show detection and logged event

10) Wrap-up
- Open `fault_log.txt` and show recent events
- Summarize results: power savings, recovery behavior, FOTA status

Notes
- If hardware power meter is not available, clearly explain and demonstrate estimation method used.
- Keep all serial/console logs visible and call out timestamps for each step.

