// SPDX-License-Identifier: GPL-2.0-or-later
// [extra] Native Linux Discord IPC worker implementation for Astral Chain PC.
#include "extra/rpc/ac_discord_rpc.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace AcDiscordRpc {
namespace {

#pragma pack(push, 1)
struct DiscordHeader {
    uint32_t opcode = 0;
    uint32_t length = 0;
};
#pragma pack(pop)

Settings g_settings;
std::thread g_worker;
std::mutex g_mutex;
std::condition_variable g_cv;
std::atomic<bool> g_stop_flag{false};
std::atomic<bool> g_connected{false};
std::atomic<bool> g_running{false};

std::string EscapeJson(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::vector<std::string> GetSocketPaths() {
    std::vector<std::string> paths;
    const char* xdg = std::getenv("XDG_RUNTIME_DIR");
    const char* tmp = std::getenv("TMPDIR");
    if (!tmp) tmp = "/tmp";
    std::string run_user = "/run/user/" + std::to_string(getuid());

    std::vector<std::string> bases;
    if (xdg && *xdg) {
        bases.push_back(xdg);
        bases.push_back(std::string(xdg) + "/app/com.discordapp.Discord");
        bases.push_back(std::string(xdg) + "/app/com.discordapp.DiscordCanary");
        bases.push_back(std::string(xdg) + "/.flatpak/com.discordapp.Discord/xdg-run");
    }
    bases.push_back(run_user);
    bases.push_back(run_user + "/app/com.discordapp.Discord");
    bases.push_back(run_user + "/app/com.discordapp.DiscordCanary");
    bases.push_back(run_user + "/.flatpak/com.discordapp.Discord/xdg-run");
    bases.push_back(tmp);

    for (const auto& b : bases) {
        for (int i = 0; i < 10; ++i) {
            paths.push_back(b + "/discord-ipc-" + std::to_string(i));
        }
    }
    return paths;
}

int ConnectSocket() {
    for (const auto& path : GetSocketPaths()) {
        struct stat st{};
        if (stat(path.c_str(), &st) != 0 || !S_ISSOCK(st.st_mode)) continue;

        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) continue;

        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        int res = connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
        if (res == 0) {
            if (flags >= 0) fcntl(fd, F_SETFL, flags);
            return fd;
        }

        if (errno == EINPROGRESS) {
            struct pollfd pfd{fd, POLLOUT, 0};
            if (poll(&pfd, 1, 150) > 0 && (pfd.revents & POLLOUT)) {
                int err = 0;
                socklen_t len = sizeof(err);
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err == 0) {
                    if (flags >= 0) fcntl(fd, F_SETFL, flags);
                    return fd;
                }
            }
        }
        close(fd);
    }
    return -1;
}

bool SendPacket(int fd, uint32_t opcode, const std::string& json) {
    DiscordHeader hdr{opcode, static_cast<uint32_t>(json.size())};
    if (send(fd, &hdr, sizeof(hdr), MSG_NOSIGNAL) != sizeof(hdr)) return false;
    return send(fd, json.data(), json.size(), MSG_NOSIGNAL) == static_cast<ssize_t>(json.size());
}

std::string BuildActivityJson(const AcGameData::PresenceSnapshot& snap) {
    auto act = GetCurrentActivity();
    std::string j = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":";
    j += std::to_string(getpid()) + ",\"activity\":{";
    j += "\"details\":\"" + EscapeJson(act.details) + "\",";
    j += "\"state\":\"" + EscapeJson(act.state) + "\"";

    j += ",\"assets\":{\"large_image\":\"" + act.large_image + "\",";
    j += "\"large_text\":\"" + EscapeJson(act.large_text) + "\"";
    if (!act.small_image.empty()) {
        j += ",\"small_image\":\"" + act.small_image + "\",";
        j += "\"small_text\":\"" + EscapeJson(act.small_text) + "\"";
    }
    j += "}";

    if (g_settings.show_time.load(std::memory_order_relaxed) && snap.session_start_unix_s > 0) {
        j += ",\"timestamps\":{\"start\":" + std::to_string(snap.session_start_unix_s) + "}";
    }

    j += "}},\"nonce\":\"" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "\"}";
    return j;
}

void WorkerLoop() {
    int fd = -1;
    while (!g_stop_flag.load(std::memory_order_relaxed)) {
        if (!g_settings.enabled.load(std::memory_order_relaxed)) {
            if (fd >= 0) {
                close(fd);
                fd = -1;
                g_connected.store(false, std::memory_order_relaxed);
            }
            std::unique_lock<std::mutex> lock(g_mutex);
            g_cv.wait_for(lock, std::chrono::seconds(1));
            continue;
        }

        if (fd < 0) {
            fd = ConnectSocket();
            if (fd >= 0) {
                std::string hs = "{\"v\":1,\"client_id\":\"" + g_settings.client_id + "\"}";
                if (!SendPacket(fd, 0, hs)) {
                    close(fd);
                    fd = -1;
                } else {
                    g_connected.store(true, std::memory_order_relaxed);
                }
            }
        }

        if (fd >= 0) {
            auto snap = AcGameData::GetSnapshot();
            std::string payload = BuildActivityJson(snap);
            if (!SendPacket(fd, 1, payload)) {
                close(fd);
                fd = -1;
                g_connected.store(false, std::memory_order_relaxed);
            }
        }

        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait_for(lock, std::chrono::seconds(fd >= 0 ? 4 : 2));
    }

    if (fd >= 0) close(fd);
    g_connected.store(false, std::memory_order_relaxed);
}

} // namespace

Settings& GetSettings() {
    return g_settings;
}

void Init() {
    if (g_running.exchange(true)) return;
    AcGameData::Init();
    g_stop_flag.store(false);
    g_worker = std::thread(WorkerLoop);
}

void Shutdown() {
    if (!g_running.exchange(false)) return;
    g_stop_flag.store(true);
    g_cv.notify_all();
    if (g_worker.joinable()) g_worker.join();
}

bool IsConnected() {
    return g_connected.load(std::memory_order_relaxed);
}

const char* GetStatusText() {
    if (!g_settings.enabled.load(std::memory_order_relaxed)) {
        return "Discord RPC: Desativado";
    }
    if (IsConnected()) {
        return "Discord RPC: Conectado ao Discord IPC";
    }
    return "Discord RPC: Procurando Discord...";
}

AcGameData::ActivityDisplay GetCurrentActivity() {
    auto snap = AcGameData::GetSnapshot();
    return AcGameData::FormatActivity(
        snap,
        g_settings.show_file.load(std::memory_order_relaxed),
        g_settings.show_mission.load(std::memory_order_relaxed),
        g_settings.show_difficulty.load(std::memory_order_relaxed),
        g_settings.show_area.load(std::memory_order_relaxed),
        g_settings.show_boss.load(std::memory_order_relaxed),
        g_settings.show_time.load(std::memory_order_relaxed)
    );
}

} // namespace AcDiscordRpc
