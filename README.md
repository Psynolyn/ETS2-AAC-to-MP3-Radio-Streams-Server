# ETS2 AAC to MP3 Radio Streams Server

A high-performance C++ local audio transcoding server and stream list updater for **Euro Truck Simulator 2 (ETS2)** and **American Truck Simulator (ATS)**.

Euro Truck Simulator 2's built-in radio player natively only supports MP3 / MPEG audio streams. Many modern radio stations stream in **AAC / AAC+ / HLS**, causing them to fail to play in-game. 

This tool seamlessly probes your radio streams, tags non-MPEG streams with `(must run server)`, hosts an HTTP stream server on `localhost:8080`, and streams live AAC radio stations auto-converted to MP3 format in real time using **FFmpeg**.

---

## Key Features

- **Plug-and-Play Executables**: Precompiled native Windows C++ binaries (`update_streams.exe` & `server.exe`) packaged in release builds.
- **Multiple `.sii` File Support**: Drop one or multiple stream files (`live_streams.sii`, `kenya_radios.sii`, etc.) into `input/`. `update_streams.exe` processes all of them at once.
- **Clear Workflow Separation**:
  - `update_streams.exe`: Scans all files in `input/`, probes codecs with `ffprobe`, builds server routes, and writes all ready-to-use `.sii` files directly into `output/`.
  - `server.exe`: Fast zero-delay startup. Loads cached routes and hosts the live AAC to MP3 audio stream server.
- **Portable FFmpeg Auto-Binding**: Automatically searches for portable `ffmpeg.exe` / `ffprobe.exe` in `./ffmpeg/`, `./`, or system `PATH`. Includes a 1-click `download_ffmpeg.bat` downloader script.
- **Smart Tagging**: Converts all occurrences of old `proxy` terminology to `server` and tags AAC stream names with `(must run server)`.
- **Multi-Threaded Codec Probing**: Fast parallel probing using `ffprobe` to categorize MPEG vs AAC streams.

---

## Repository Structure

```
ETS2-AAC-to-MP3-Radio-Streams-Server/
├── ffmpeg/              # Portable ffmpeg.exe & ffprobe.exe location
├── input/               # Place input live_streams.sii or multiple stream files here
│   └── live_streams.sii # Sample stream list with 5 popular global AAC radio stations
├── src/                 # Source code
│   ├── common.h         # Shared path resolution, parsing & string utilities
│   ├── update_streams.cpp # Multi-file codec probe & output generator
│   └── server.cpp       # HTTP audio transcoding server
├── build.bat            # 1-click release package builder
├── download_ffmpeg.bat  # 1-click portable FFmpeg downloader
├── LICENSE              # License
└── README.md            # Documentation
```

---

## Quick Start Guide

### Step 1: Ensure FFmpeg is Available
- If you already have `ffmpeg` installed in system `PATH`, you're ready.
- If not, double-click `download_ffmpeg.bat` to automatically download and extract portable `ffmpeg.exe` and `ffprobe.exe` into the `ffmpeg/` folder.

### Step 2: Prepare Input Stream File(s)
- Place your `live_streams.sii` file (or multiple `.sii` / `.txt` files) into the `input/` folder.

### Step 3: Run `update_streams.exe`
- Double-click `update_streams.exe`.
- It will scan all files in `input/`, probe stream audio codecs, append `(must run server)` to AAC streams, build routing tables in `processed_cache/server_routes.txt`, and generate all final ready-to-use `.sii` files in the `output/` folder.

### Step 4: Run `server.exe`
- Double-click `server.exe`.
- The server will immediately load all active stream routes and start the HTTP audio server on `http://localhost:8080`.
- Keep `server.exe` running while playing ETS2/ATS.

### Step 5: Copy Output to ETS2 Profile
- Copy your prepared `.sii` file (e.g. `live_streams.sii`) from `output/` into your Euro Truck Simulator 2 profile folder:
  `Documents\Euro Truck Simulator 2\` (or `Documents\American Truck Simulator\`).
- Launch ETS2, open the in-game Radio player, and enjoy listening to all your radio stations!

---

## Windows Security Notice (Unknown Publisher)

Because `update_streams.exe` and `server.exe` are open-source C++ executables compiled without a paid commercial Code Signing Certificate, Windows SmartScreen or Defender may display an **"Unknown Publisher"** or **"Windows protected your PC"** warning on first launch.

To run the executables:
1. Click **More info** on the Windows SmartScreen dialog.
2. Click **Run anyway**.
*(Alternatively, right-click the `.exe` file, select **Properties**, check **Unblock** at the bottom, and click **Apply**).*

---

## Compiling from Source

If you wish to modify the C++ code or recompile release binaries using MinGW-w64:

```cmd
build.bat
```

Or manually using `g++`:

```cmd
g++ -O2 -std=c++17 src/update_streams.cpp -o update_streams.exe
g++ -O2 -std=c++17 src/server.cpp -o server.exe -lws2_32
```

---

## License

MIT License. Free for all ETS2 & ATS sim drivers and modders!