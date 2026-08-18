#include "common.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <future>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

constexpr int PORT = 8080;

struct StationRoute {
    std::string safe_name;
    std::string original_url;
    std::string display_name;
};

namespace fs_util {
    inline bool create_dir_if_missing(const std::string& path) {
#ifdef _WIN32
        return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
        return system(("mkdir -p " + path).c_str()) == 0;
#endif
    }

    inline std::vector<std::string> get_files_in_dir(const std::string& dir) {
        std::vector<std::string> files;
#ifdef _WIN32
        std::string search_path = dir + "\\*.*";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = ::FindFirstFileA(search_path.c_str(), &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    files.push_back(fd.cFileName);
                }
            } while (::FindNextFileA(hFind, &fd));
            ::FindClose(hFind);
        }
#endif
        return files;
    }
}

class MultiStreamUpdater {
private:
    std::mutex print_mutex;
    std::mutex routes_mutex;

    std::map<std::string, StationRoute> master_routes;
    std::string ffprobe_bin;

    std::string check_codec(const std::string& url, int timeout_sec = 5) {
        if (url.empty() || url.find("URL_HERE") != std::string::npos || url.substr(0, 4) != "http") {
            return "unknown";
        }

        std::ostringstream cmd;
        if (ffprobe_bin.find(' ') != std::string::npos) {
            cmd << "\"" << ffprobe_bin << "\"";
        } else {
            cmd << ffprobe_bin;
        }

        cmd << " -v error -select_streams a:0 "
            << "-show_entries stream=codec_name "
            << "-of default=noprint_wrappers=1:nokey=1 "
            << "-timeout " << (timeout_sec * 1000000) << " "
            << "-analyzeduration 2000000 "
            << "-probesize 1000000 "
            << "\"" << url << "\"";

#ifdef _WIN32
        cmd << " 2>NUL";
#else
        cmd << " 2>/dev/null";
#endif

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) return "unknown";

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        result = ets2_radio::trim(result);
        result = ets2_radio::to_lower(result);

        if (result == "mp3" || result == "mp2" || result == "mp1") {
            return "mpeg";
        } else if (result == "aac" || result == "aac_latm") {
            return "aac";
        } else {
            return result.empty() ? "unknown" : result;
        }
    }

    struct StreamProcessingResult {
        size_t index;
        std::string output_line;
        bool is_mpeg = false;
        StationRoute route;
    };

    StreamProcessingResult process_single_stream(size_t index, int total, const std::string& line, const std::string& filename) {
        StreamProcessingResult res;
        res.index = index;
        res.output_line = line;

        std::string trimmed = ets2_radio::trim(line);
        ets2_radio::StreamLine parsed = ets2_radio::parse_stream_line(trimmed);

        if (!parsed.valid || parsed.url.empty()) {
            return res;
        }

        std::string display_name = parsed.station_name.empty() ? "Radio Station" : parsed.station_name;

        {
            std::lock_guard<std::mutex> lock(print_mutex);
            std::cout << "[" << filename << "] [" << (index + 1) << "/" << total << "] Probing " << display_name << "..." << std::endl;
        }

        std::string codec = check_codec(parsed.url);

        if (codec == "mpeg") {
            res.is_mpeg = true;
            // MP3/MPEG stream: UNTOUCHED! Do NOT add (must run server), do NOT route through localhost server
            res.output_line = line;

            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "  ✓ Direct MP3/MPEG stream detected -> UNTOUCHED (runs directly in ETS2)" << std::endl;
            }
        } else {
            res.is_mpeg = false;
            std::string tagged_line = ets2_radio::add_server_note(line);

            {
                std::lock_guard<std::mutex> lock(print_mutex);
                std::cout << "  → Non-MPEG stream detected (" << codec << ") -> Routed through Server" << std::endl;
            }

            // Generate safe route name
            std::string safe_name = ets2_radio::make_safe_name(parsed.station_name);

            {
                std::lock_guard<std::mutex> lock(routes_mutex);
                std::string base_safe = safe_name;
                int counter = 1;
                while (master_routes.find(safe_name) != master_routes.end() &&
                       master_routes[safe_name].original_url != parsed.url) {
                    safe_name = base_safe + "_" + std::to_string(counter++);
                }

                std::string tagged_display_name = parsed.station_name;
                if (tagged_display_name.find("(must run server)") == std::string::npos) {
                    tagged_display_name += " (must run server)";
                }

                res.route = {safe_name, parsed.url, tagged_display_name};
                master_routes[safe_name] = res.route;
            }

            // Replace URL in stream_data line with localhost URL
            size_t url_start = tagged_line.find('"');
            size_t pipe_pos = tagged_line.find('|');
            if (url_start != std::string::npos && pipe_pos != std::string::npos && url_start < pipe_pos) {
                std::string localhost_url = "http://localhost:" + std::to_string(PORT) + "/" + res.route.safe_name;
                res.output_line = tagged_line.substr(0, url_start + 1) + localhost_url + tagged_line.substr(pipe_pos);
            } else {
                res.output_line = tagged_line;
            }
        }

        return res;
    }

public:
    MultiStreamUpdater(const std::string& ffprobe_path) : ffprobe_bin(ffprobe_path) {}

    bool process_all_files(const std::vector<std::string>& files) {
        fs_util::create_dir_if_missing("output");
        fs_util::create_dir_if_missing("processed_cache");

        int total_files_processed = 0;
        int total_streams_processed = 0;
        int mpeg_count = 0;
        int non_mpeg_count = 0;

        for (const auto& filename : files) {
            std::string input_path = "input\\" + filename;
            std::ifstream infile(input_path);
            if (!infile.is_open()) {
                std::cerr << "WARNING: Could not open " << input_path << std::endl;
                continue;
            }

            std::vector<std::string> raw_lines;
            std::string line;
            while (std::getline(infile, line)) {
                raw_lines.push_back(line);
            }
            infile.close();

            std::vector<std::string> header_lines;
            std::vector<std::string> stream_lines;
            std::vector<std::string> footer_lines;

            bool found_stream = false;

            for (const auto& l : raw_lines) {
                std::string trimmed = ets2_radio::trim(l);
                ets2_radio::StreamLine parsed = ets2_radio::parse_stream_line(trimmed);

                if (parsed.valid) {
                    found_stream = true;
                    stream_lines.push_back(l);
                } else if (!found_stream) {
                    header_lines.push_back(l);
                } else {
                    footer_lines.push_back(l);
                }
            }

            int file_total = stream_lines.size();
            std::cout << "\n--------------------------------------------------------" << std::endl;
            std::cout << " Processing File: input/" << filename << " (" << file_total << " streams)" << std::endl;
            std::cout << "--------------------------------------------------------" << std::endl;

            int max_workers = 10;
            std::vector<std::future<StreamProcessingResult>> futures;

            for (size_t i = 0; i < stream_lines.size(); ++i) {
                futures.push_back(
                    std::async(std::launch::async,
                               &MultiStreamUpdater::process_single_stream,
                               this, i, file_total, stream_lines[i], filename)
                );

                if (futures.size() >= static_cast<size_t>(max_workers)) {
                    futures.front().wait();
                }
            }

            std::vector<std::string> processed_stream_lines(file_total);
            for (auto& fut : futures) {
                StreamProcessingResult res = fut.get();
                if (res.index < processed_stream_lines.size()) {
                    processed_stream_lines[res.index] = res.output_line;
                    if (res.is_mpeg) {
                        mpeg_count++;
                    } else {
                        non_mpeg_count++;
                    }
                }
            }

            // Default headers if file had no header
            if (header_lines.empty()) {
                header_lines.push_back("SiiNunit");
                header_lines.push_back("{");
                header_lines.push_back("live_stream_def : .live_streams {");
            }
            if (footer_lines.empty()) {
                footer_lines.push_back("}");
                footer_lines.push_back("}");
            }

            // Write final output file directly to output/ folder
            std::string output_path = "output\\" + filename;
            std::ofstream outfile(output_path);

            bool count_updated = false;

            for (const auto& h : header_lines) {
                std::string trimmed = ets2_radio::trim(h);
                if (trimmed.rfind("stream_data:", 0) == 0) {
                    outfile << " stream_data: " << file_total << "\n";
                    count_updated = true;
                } else {
                    outfile << h << "\n";
                }
            }

            if (!count_updated) {
                outfile << " stream_data: " << file_total << "\n";
            }

            for (const auto& sl : processed_stream_lines) {
                outfile << " " << ets2_radio::trim(sl) << "\n";
            }

            for (const auto& f : footer_lines) {
                outfile << f << "\n";
            }

            outfile.close();

            std::cout << " [GENERATED] -> output/" << filename << " (" << file_total << " streams prepared)" << std::endl;

            total_files_processed++;
            total_streams_processed += file_total;
        }

        // Save master server routes (ONLY non-MPEG streams) to processed_cache/server_routes.txt
        std::string routes_file = "processed_cache/server_routes.txt";
        std::ofstream rfile(routes_file);
        for (const auto& pair : master_routes) {
            rfile << pair.second.safe_name << "|"
                  << pair.second.original_url << "|"
                  << pair.second.display_name << "\n";
        }
        rfile.close();

        std::cout << "\n========================================================" << std::endl;
        std::cout << " ALL INPUT FILES PROCESSED SUCCESSFULLY!" << std::endl;
        std::cout << " Total Files Processed:  " << total_files_processed << std::endl;
        std::cout << " Total Streams Analyzed: " << total_streams_processed << std::endl;
        std::cout << " Direct MP3 Streams:     " << mpeg_count << " (Untouched, played directly in ETS2)" << std::endl;
        std::cout << " Non-MP3 Server Routes:  " << master_routes.size() << " saved to processed_cache/server_routes.txt" << std::endl;
        std::cout << " Prepared Output Files:  Written to 'output/' folder" << std::endl;
        std::cout << "========================================================" << std::endl;
        std::cout << " You can now run 'server.exe' to host the non-MP3 streams." << std::endl;
        std::cout << "========================================================\n" << std::endl;

        return true;
    }
};

bool check_ffprobe_executable(const std::string& ffprobe_path) {
    std::string cmd = "\"" + ffprobe_path + "\" -version";
#ifdef _WIN32
    cmd += " >NUL 2>&1";
#else
    cmd += " >/dev/null 2>&1";
#endif
    return (system(cmd.c_str()) == 0);
}

int main() {
    fs_util::create_dir_if_missing("input");
    fs_util::create_dir_if_missing("output");
    fs_util::create_dir_if_missing("processed_cache");
    fs_util::create_dir_if_missing("ffmpeg");

    std::string ffprobe_path = ets2_radio::get_ffprobe_path();
    if (!check_ffprobe_executable(ffprobe_path)) {
        std::cerr << "========================================================\n"
                  << " ERROR: ffprobe executable not found!\n"
                  << " Checked path: " << ffprobe_path << "\n\n"
                  << " Please place 'ffmpeg.exe' and 'ffprobe.exe' in 'ffmpeg/' folder\n"
                  << " or run 'download_ffmpeg.bat' to download them automatically.\n"
                  << "========================================================" << std::endl;
        return 1;
    }

    std::vector<std::string> all_files = fs_util::get_files_in_dir("input");
    std::vector<std::string> stream_files;

    for (const auto& file : all_files) {
        if (file.rfind(".sii") != std::string::npos || file.rfind(".txt") != std::string::npos) {
            stream_files.push_back(file);
        }
    }

    if (stream_files.empty()) {
        std::cerr << "========================================================\n"
                  << " ERROR: No input stream files (.sii / .txt) found in 'input/'!\n\n"
                  << " Please copy your stream file(s) into the 'input/' folder\n"
                  << " and run update_streams again.\n"
                  << "========================================================" << std::endl;
        return 1;
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << " ETS2 MULTI-FILE STREAM UPDATER" << std::endl;
    std::cout << " Found " << stream_files.size() << " input file(s) in 'input/':" << std::endl;
    for (const auto& sf : stream_files) {
        std::cout << "  - input/" << sf << std::endl;
    }
    std::cout << "========================================================" << std::endl;

    MultiStreamUpdater updater(ffprobe_path);
    if (!updater.process_all_files(stream_files)) {
        return 1;
    }

    return 0;
}
