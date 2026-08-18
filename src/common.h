#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ets2_radio {

// Struct representing a parsed ETS2 stream line
struct StreamLine {
    std::string full_line;
    std::string url;
    std::string station_name;
    std::string rest; // Genre|Language|Bitrate|Fav etc.
    bool valid = false;
};

// Trim leading and trailing whitespace
inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    size_t end = str.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return str.substr(start, end - start + 1);
}

// Convert string to lower case
inline std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

// Check if a file exists on Windows
inline bool file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD dwAttrib = GetFileAttributesA(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    std::ifstream f(path.c_str());
    return f.good();
#endif
}

// Portable resolution for ffmpeg.exe path (checks ffmpeg/ folder first)
inline std::string get_ffmpeg_path() {
    if (file_exists("ffmpeg\\ffmpeg.exe")) return "ffmpeg\\ffmpeg.exe";
    if (file_exists("ffmpeg/ffmpeg.exe")) return "ffmpeg/ffmpeg.exe";
    if (file_exists("ffmpeg\\bin\\ffmpeg.exe")) return "ffmpeg\\bin\\ffmpeg.exe";
    if (file_exists("ffmpeg/bin/ffmpeg.exe")) return "ffmpeg/bin/ffmpeg.exe";
    if (file_exists("bin\\ffmpeg.exe")) return "bin\\ffmpeg.exe";
    if (file_exists("bin/ffmpeg.exe")) return "bin/ffmpeg.exe";
    if (file_exists("ffmpeg.exe")) return "ffmpeg.exe";
    return "ffmpeg";
}

// Portable resolution for ffprobe.exe path (checks ffmpeg/ folder first)
inline std::string get_ffprobe_path() {
    if (file_exists("ffmpeg\\ffprobe.exe")) return "ffmpeg\\ffprobe.exe";
    if (file_exists("ffmpeg/ffprobe.exe")) return "ffmpeg/ffprobe.exe";
    if (file_exists("ffmpeg\\bin\\ffprobe.exe")) return "ffmpeg\\bin\\ffprobe.exe";
    if (file_exists("ffmpeg/bin/ffprobe.exe")) return "ffmpeg/bin/ffprobe.exe";
    if (file_exists("bin\\ffprobe.exe")) return "bin\\ffprobe.exe";
    if (file_exists("bin/ffprobe.exe")) return "bin/ffprobe.exe";
    if (file_exists("ffprobe.exe")) return "ffprobe.exe";
    return "ffprobe";
}

// Parse stream line matching standard ETS2 live_streams.sii format: stream_data[]: "URL|Name|Genre|Lang|Bitrate|Fav"
inline StreamLine parse_stream_line(const std::string& line) {
    StreamLine result;
    result.full_line = line;
    result.valid = false;

    std::regex pattern(R"foo(stream_data(?:\[\d*\])?: "([^|]+)\|(.+)")foo");
    std::smatch match;

    if (std::regex_search(line, match, pattern)) {
        result.url = trim(match[1].str());
        result.rest = match[2].str();

        size_t pos = result.rest.find('|');
        if (pos != std::string::npos) {
            result.station_name = trim(result.rest.substr(0, pos));
        } else {
            result.station_name = trim(result.rest);
        }

        result.valid = true;
    }

    return result;
}

// Generate URL-safe ASCII name for radio station
inline std::string make_safe_name(const std::string& station_name) {
    std::string safe = station_name;

    // Remove "(must run server)" or "(must run proxy)" if present in name
    size_t note_pos = safe.find("(must run");
    if (note_pos != std::string::npos) {
        safe = safe.substr(0, note_pos);
    }

    safe.erase(std::remove_if(safe.begin(), safe.end(),
        [](char c) { return !std::isalnum(static_cast<unsigned char>(c)) && c != ' ' && c != '-'; }),
        safe.end());

    std::replace(safe.begin(), safe.end(), ' ', '_');
    safe = to_lower(safe);

    size_t start = safe.find_first_not_of('_');
    size_t end = safe.find_last_not_of('_');
    if (start != std::string::npos && end != std::string::npos) {
        safe = safe.substr(start, end - start + 1);
    } else if (safe.empty()) {
        safe = "radio_stream";
    }

    if (safe.length() > 50) {
        safe = safe.substr(0, 50);
    }

    return safe;
}

// Add "(must run server)" tag to station name inside stream_data string
inline std::string add_server_note(const std::string& line) {
    size_t first_pipe = line.find('|');
    if (first_pipe == std::string::npos) return line;

    size_t second_pipe = line.find('|', first_pipe + 1);
    if (second_pipe == std::string::npos) return line;

    std::string station_name = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);

    // If it already has (must run proxy) or (must run server), normalize it to (must run server)
    size_t proxy_pos = station_name.find("(must run proxy)");
    if (proxy_pos != std::string::npos) {
        station_name.replace(proxy_pos, 16, "(must run server)");
        return line.substr(0, first_pipe + 1) + station_name + line.substr(second_pipe);
    }

    if (station_name.find("(must run server)") != std::string::npos) {
        return line;
    }

    station_name = trim(station_name);
    station_name += " (must run server)";

    return line.substr(0, first_pipe + 1) + station_name + line.substr(second_pipe);
}

} // namespace ets2_radio

#endif // COMMON_H
