# ETS2 AAC to MP3 Radio Streams Server

A local audio transcoding server and stream list updater for **Euro Truck Simulator 2 (ETS2)** and **American Truck Simulator (ATS)**.

Euro Truck Simulator 2's built-in radio player natively only supports MP3 / MPEG audio streams. Many modern radio stations stream in **AAC / AAC+ / HLS**, causing them to fail to play in-game. 

This tool seamlessly probes your radio streams, tags non-MPEG streams with `(must run server)`, hosts an HTTP stream server on `localhost:8080`, and streams live AAC radio stations auto-converted to MP3 format in real time using **FFmpeg**.

##Repository Structure

```
ETS2-AAC-to-MP3-Radio-Streams-Server/
├── bin/                 # Portable ffmpeg.exe & ffprobe.exe location
├── input/               # Place input live_streams.sii or multiple stream files here
│   ├── live_streams.sii # Sample stream list with 5 AAC radio stations
├── output/              # Final prepared .sii files generated here
├── processed_cache/     # Server routes & stream cache (server_routes.txt)
├── src/                 # Source code
│   ├── common.h         # Shared path resolution, parsing & string utilities
│   ├── update_streams.cpp # Multi-file codec probe & output generator
│   └── server.cpp       # HTTP audio transcoding server
├── build.bat            # MinGW g++ compilation script
├── download_ffmpeg.bat  # FFmpeg downloader
├── update_streams.exe   # Pre-compiled executable for stream updating
├── server.exe           # Pre-compiled executable for stream hosting
└── README.md            # Documentation
```

---

## 🚀 Quick Start Guide

### Step 1: Ensure FFmpeg is Available
- If you already have `ffmpeg` installed in system `PATH`, you're ready!
- If not, double-click `download_ffmpeg.bat` to automatically download and extract portable `ffmpeg.exe` and `ffprobe.exe` into the `bin/` folder.

### Step 2: Prepare Input Stream File(s)
- Place your `live_streams.sii` file (or multiple `.sii` files) into the `input/` folder.

### Step 3: Run `update_streams.exe`
- Double-click `update_streams.exe`.
- It will scan all files in `input/`, probe stream audio codecs, append `(must run server)` to AAC streams, build routing tables in `processed_cache/server_routes.txt`, and generate all final ready-to-use `.sii` files in the `output/` folder!

### Step 4: Run `server.exe`
- Double-click `server.exe`.
- The server will immediately load all active stream routes and start the HTTP audio server on `http://localhost:8080`.
- Keep `server.exe` running while playing ETS2/ATS!

### Step 5: Copy Output to ETS2 Profile
- Copy your prepared `.sii` file (e.g. `live_streams.sii`) from `output/` into your Euro Truck Simulator 2 profile folder:
  `Documents\Euro Truck Simulator 2\` (or `Documents\American Truck Simulator\`).
- Launch ETS2, open the in-game Radio player, and enjoy listening to all your radio stations!

---

##Compiling from Source

If you wish to modify the C++ code or recompile binaries using MinGW-w64:

```cmd
build.bat
```

Or manually using `g++`:

```cmd
g++ -O2 -std=c++17 src/update_streams.cpp -o update_streams.exe
g++ -O2 -std=c++17 src/server.cpp -o server.exe -lws2_32
```

---

##License

All files are subject to MIT License.