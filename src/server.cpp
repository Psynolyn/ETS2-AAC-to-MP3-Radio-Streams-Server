#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include "common.h"
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT = 8080;
constexpr size_t BUFFER_SIZE = 8192;

struct StationRouteInfo {
    std::string original_url;
    std::string display_name;
};

std::map<std::string, StationRouteInfo> radio_routes;
std::atomic<bool> server_running{true};
std::vector<HANDLE> active_processes;
CRITICAL_SECTION process_lock;
std::string ffmpeg_bin_path;

int load_server_routes(const std::string& filename = "processed_cache/server_routes.txt") {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return 0;
    }

    int loaded = 0;
    std::string line;

    while (std::getline(file, line)) {
        line = ets2_radio::trim(line);
        if (line.empty()) continue;

        size_t first_pipe = line.find('|');
        size_t second_pipe = (first_pipe != std::string::npos) ? line.find('|', first_pipe + 1) : std::string::npos;

        if (first_pipe != std::string::npos && second_pipe != std::string::npos) {
            std::string safe_name = line.substr(0, first_pipe);
            std::string original_url = line.substr(first_pipe + 1, second_pipe - first_pipe - 1);
            std::string display_name = line.substr(second_pipe + 1);

            radio_routes[safe_name] = {original_url, display_name};
            loaded++;
        }
    }

    file.close();
    return loaded;
}

void print_server_dashboard() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << " ETS2 AAC->MP3 AUDIO TRANSCODING SERVER" << std::endl;
    std::cout << " Base URL: http://localhost:" << PORT << std::endl;
    std::cout << " FFmpeg Executable: " << ffmpeg_bin_path << std::endl;
    std::cout << "========================================================\n" << std::endl;

    std::cout << "Hosted AAC Radio Streams (" << radio_routes.size() << " active routes):\n" << std::endl;
    std::cout << std::left << std::setw(38) << "Station Name"
              << std::setw(42) << "Localhost Stream URL" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& entry : radio_routes) {
        std::string name = entry.second.display_name;
        if (name.length() > 36) name = name.substr(0, 36);

        std::string localhost_url = "http://localhost:" + std::to_string(PORT) + "/" + entry.first;
        std::cout << std::left << std::setw(38) << name
                  << std::setw(42) << localhost_url << std::endl;
    }

    std::cout << "\n========================================================" << std::endl;
    std::cout << " Server is RUNNING. Copy your prepared .sii file(s) from" << std::endl;
    std::cout << " the 'output/' folder into your ETS2 documents profile." << std::endl;
    std::cout << " Press Ctrl+C to stop the server." << std::endl;
    std::cout << "========================================================\n" << std::endl;
}

bool stream_radio_as_mpeg(SOCKET client_socket, const std::string& url) {
    std::string cmd = "\"" + ffmpeg_bin_path + "\" -i \"" + url + "\" -vn -acodec libmp3lame -ab 128k -ar 44100 -ac 2 -f mp3 -";

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE hRead, hWrite;

    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        return false;
    }

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {0};

    if (!CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE,
                        CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(hWrite);

    EnterCriticalSection(&process_lock);
    active_processes.push_back(pi.hProcess);
    LeaveCriticalSection(&process_lock);

    char buffer[BUFFER_SIZE];
    DWORD bytes_read;

    while (ReadFile(hRead, buffer, BUFFER_SIZE, &bytes_read, NULL) && bytes_read > 0) {
        int sent = send(client_socket, buffer, bytes_read, 0);
        if (sent <= 0) break;
    }

    CloseHandle(hRead);
    TerminateProcess(pi.hProcess, 0);

    EnterCriticalSection(&process_lock);
    auto it = std::find(active_processes.begin(), active_processes.end(), pi.hProcess);
    if (it != active_processes.end()) {
        active_processes.erase(it);
    }
    LeaveCriticalSection(&process_lock);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}

void handle_client(SOCKET client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_read = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read <= 0) {
        closesocket(client_socket);
        return;
    }

    buffer[bytes_read] = '\0';
    std::string request(buffer);

    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }

    // Decode URL hex escapes (%20, etc.)
    size_t pos = 0;
    while ((pos = path.find('%', pos)) != std::string::npos && pos + 2 < path.length()) {
        std::string hex = path.substr(pos + 1, 2);
        char decoded = static_cast<char>(std::stoi(hex, nullptr, 16));
        path.replace(pos, 3, 1, decoded);
        pos++;
    }

    auto it = radio_routes.find(path);
    if (it != radio_routes.end()) {
        std::cout << "[CLIENT CONNECTED] Transcoding stream: " << it->second.display_name << std::endl;
        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: audio/mpeg\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: keep-alive\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n";

        send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
        stream_radio_as_mpeg(client_socket, it->second.original_url);
    } else {
        std::string response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 22\r\n"
            "\r\n"
            "Radio station not found";

        send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
    }

    closesocket(client_socket);
}

void run_server() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed on port " << PORT << ". Is another server process running?" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    while (server_running) {
        sockaddr_in client_addr = {0};
        int client_len = sizeof(client_addr);

        SOCKET client_socket = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            if (server_running) {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        std::thread(handle_client, client_socket).detach();
    }

    closesocket(server_fd);
    WSACleanup();
}

BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        std::cout << "\n\nShutting down radio server..." << std::endl;

        EnterCriticalSection(&process_lock);
        for (HANDLE proc : active_processes) {
            TerminateProcess(proc, 0);
            CloseHandle(proc);
        }
        active_processes.clear();
        LeaveCriticalSection(&process_lock);

        server_running = false;
        Sleep(500);
        exit(0);
    }
    return TRUE;
}

bool check_ffmpeg_executable(const std::string& ffmpeg_path) {
    std::string cmd = "\"" + ffmpeg_path + "\" -version >NUL 2>&1";
    return (system(cmd.c_str()) == 0);
}

int main() {
    InitializeCriticalSection(&process_lock);

    ffmpeg_bin_path = ets2_radio::get_ffmpeg_path();
    if (!check_ffmpeg_executable(ffmpeg_bin_path)) {
        std::cerr << "========================================================\n"
                  << " ERROR: ffmpeg executable not found!\n"
                  << " Checked path: " << ffmpeg_bin_path << "\n\n"
                  << " Please place 'ffmpeg.exe' in 'bin/' directory or run\n"
                  << " 'download_ffmpeg.bat' to download it automatically.\n"
                  << "========================================================" << std::endl;
        DeleteCriticalSection(&process_lock);
        return 1;
    }

    int loaded = load_server_routes();
    if (loaded == 0) {
        std::cerr << "========================================================\n"
                  << " ERROR: No server routes found in cache!\n\n"
                  << " Please run 'update_streams.exe' first to scan your\n"
                  << " 'input/' stream file(s) and generate the prepared files.\n"
                  << "========================================================" << std::endl;
        DeleteCriticalSection(&process_lock);
        return 1;
    }

    SetConsoleCtrlHandler(console_handler, TRUE);
    print_server_dashboard();
    run_server();

    DeleteCriticalSection(&process_lock);
    return 0;
}
