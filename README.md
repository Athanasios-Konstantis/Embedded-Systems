## Embedded-Systems — Raspberry Pi Transaction Ingestor

This application runs indefinitely on a Raspberry Pi, connecting to a websocket to receive live transaction data and storing it locally. It is designed for 24/7 operation with simple deployment and cross-compilation from a development machine.

Operational overview:
- Connects to a remote websocket endpoint and subscribes to transaction streams.
- Parses messages and persists them (e.g., to files or a simple database as implemented in `apiconn.c`).
- Runs forever until stopped; on network issues, restart or implement reconnection logic.
- For service management and logging, consider `systemd` and redirecting stdout/stderr to log files.

This repository contains C code intended to run on a Raspberry Pi. The project is designed to be cross-compiled on a development machine (Windows or Linux) and deployed to the Pi. It also includes Python scripts for generating graphs from collected data.

### Repository structure
- `apiconn.c` — Main C source for the Raspberry Pi app
- `Makefile` — Build rules (supports cross-compilation)
- `python_code/` — Python utilities:
	- `graphs.py`, `graphs2.py`, `graphs3.py`
- `figures/` — Figures and assets
- `Good_Data.zip` — Sample dataset
- `Embedded_Report_Athanasios_Konstantis_10537.pdf` — Project report

## Prerequisites

On the Raspberry Pi:
- Raspberry Pi OS (32-bit recommended) with SSH enabled
- Network connectivity (same LAN or reachable via IP)

On the development machine (Windows):
- Windows PowerShell
- ARM cross-compiler toolchain (e.g., GCC for ARM)
	- Option A: Install via MSYS2 and `arm-none-eabi-gcc` (no sysroot; best for bare-metal)
	- Option B: Use a Linux VM/WSL with `gcc-arm-linux-gnueabihf` (recommended for Raspberry Pi userland)
- Optional: `pscp` (PuTTY SCP) for file transfer, or use `scp` from WSL/MSYS2

On Linux/macOS development machines:
- `gcc-arm-linux-gnueabihf` (Debian/Ubuntu)
- `rsync`/`scp`

Note on toolchain choice:
- Raspberry Pi OS userland typically uses `arm-linux-gnueabihf` (hard-float, 32-bit). Use that triple for compatibility.

## Build (cross-compilation)

The provided `Makefile` is expected to support cross-compiling. If needed, you can pass a compiler and flags explicitly.

### Windows (PowerShell) using WSL Ubuntu — recommended
1. Install WSL and Ubuntu.
2. In Ubuntu, install the cross compiler:
	 - `sudo apt update && sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf rsync`
3. From Ubuntu, navigate to the project directory (mounted under `/mnt/c/...`).
4. Build:
	 - `make CC=arm-linux-gnueabihf-gcc`

### Linux host
```bash
sudo apt update && sudo apt install -y gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
make CC=arm-linux-gnueabihf-gcc
```

### Notes
- If headers/libraries from the Pi are required (e.g., wiringPi or other system libs), set `SYSROOT` to a copy of the Pi filesystem or use `rsync` to mirror `/usr` and `/lib` from the Pi, then add `--sysroot=$SYSROOT` and appropriate `-I`/`-L` paths.
- If the program is pure standard C and does not depend on Pi-specific libraries, the default cross-compiler and glibc targeting `arm-linux-gnueabihf` is sufficient.

## Configuration

If the websocket URL or storage path is configurable, set them via command-line arguments, compile-time macros in `apiconn.c`, or environment variables. For persistent service management, use `systemd` on the Pi to keep it alive and auto-restart on failure.

## Python graph scripts

The `python_code/` folder contains helper scripts for plotting/processing data:

- `graphs.py`
- `graphs2.py`
- `graphs3.py`

### Requirements
- Python 3.9+
- Common libraries (likely `matplotlib`, `numpy`, `pandas`) — install as needed.
## Notes

- If the `Makefile` produces a different binary name or output folder, adjust copy/run commands accordingly.
- For reproducibility, prefer building from WSL Ubuntu and scp to the Pi.

