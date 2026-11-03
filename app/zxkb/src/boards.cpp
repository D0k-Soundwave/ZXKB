// Keyboard definitions for plain VIA keyboards. Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "boards.h"
#include "json.h"
#include "settings.h"
#include <windows.h>
#include <winhttp.h>
#include <cwchar>

namespace boards {
namespace {

// [P] VIA's web app loads compiled definitions from usevia.app/definitions/v3/<vendor ID x 65536 + product ID>.json
// (checked 2026-11-03 with the Q3: 10,470 bytes). Not a documented service: it may move.
const wchar_t VIA_HOST[] = L"usevia.app";
constexpr DWORD NET_TIMEOUT_MS = 15000; // [A] a 10 KB download normally takes well under a second
constexpr DWORD MAX_DOWNLOAD   = 4 * 1024 * 1024; // [A] the largest definitions are tens of KB

std::wstring folder() {
    std::wstring f = settings::folder() + L"\\boards";
    CreateDirectoryW(f.c_str(), nullptr);
    return f;
}

std::wstring via_path(uint16_t vid, uint16_t pid) {
    wchar_t name[40];
    swprintf(name, 40, L"\\%04X_%04X.via.json", vid, pid);
    return folder() + name;
}

bool read_file(const std::wstring &path, std::string *out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(f, nullptr), n = 0;
    bool  ok   = size > 0 && size <= MAX_DOWNLOAD;
    if (ok) {
        out->resize(size);
        ok = ReadFile(f, &(*out)[0], size, &n, nullptr) && n == size;
    }
    CloseHandle(f);
    return ok;
}

bool write_file(const std::wstring &path, const std::string &data) {
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD n = 0;
    bool  ok = WriteFile(f, data.data(), (DWORD)data.size(), &n, nullptr) && n == data.size();
    CloseHandle(f);
    return ok && MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// The menu entry whose "content" starts with the given id, e.g. ["id_qmk_rgb_matrix_effect", 3, 2]
const Json *find_control(const Json &j, const char *id) {
    if (j.is(Json::OBJ)) {
        const Json &c = j["content"];
        if (c.is(Json::ARR) && c[0].is(Json::STR) && c[0].str == id) return &j;
        for (const auto &m : j.members)
            if (const Json *r = find_control(m.second, id)) return r;
    } else if (j.is(Json::ARR)) {
        for (const Json &i : j.items)
            if (const Json *r = find_control(i, id)) return r;
    }
    return nullptr;
}

bool get(const wchar_t *host, const std::wstring &path, std::string *out, std::wstring *error) {
    bool ok = false;
    HINTERNET s = WinHttpOpen(L"ZXKB", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    if (r) {
        WinHttpSetTimeouts(r, NET_TIMEOUT_MS, NET_TIMEOUT_MS, NET_TIMEOUT_MS, NET_TIMEOUT_MS);
        if (WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            if (status == 200) {
                char  buf[8192];
                DWORD n = 0;
                ok = true;
                while (WinHttpReadData(r, buf, sizeof(buf), &n) && n) {
                    out->append(buf, n);
                    if (out->size() > MAX_DOWNLOAD) { ok = false; break; }
                }
            } else if (status == 404) {
                *error = L"VIA has no definition for this keyboard.";
            } else {
                *error = L"The server answered " + std::to_wstring(status) + L".";
            }
        } else {
            *error = L"Could not reach " + std::wstring(host) + L" (no internet connection?).";
        }
    } else {
        *error = L"Could not start a download.";
    }
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return ok;
}

} // namespace

std::wstring definition_source() { return VIA_HOST; }

bool has_definition(const Board &b) { return GetFileAttributesW(via_path(b.vid, b.pid).c_str()) != INVALID_FILE_ATTRIBUTES; }

bool apply_definition(Board *b) {
    std::string text;
    if (!read_file(via_path(b->vid, b->pid), &text)) return false;
    Json def = Json::parse(text);
    if (!def.is(Json::OBJ)) return false;
    b->def_name = settings::widen(def["name"].to_str());
    // lighting: RGB matrix (VIA channel 3) or RGB light (channel 2)
    const Json *fx = find_control(def["menus"], "id_qmk_rgb_matrix_effect");
    if (!fx) fx = find_control(def["menus"], "id_qmk_rgblight_effect");
    b->names.clear();
    b->kinds.clear();
    if (!fx) { b->via_channel = 0; return true; } // no lighting the app can drive
    b->via_channel = (uint8_t)(*fx)["content"][1].to_int(3);
    for (const Json &o : (*fx)["options"].items) { // ["01. SOLID_COLOR", 1]
        int value = o[1].to_int(-1);
        if (value < 1 || value > 255) continue;   // 0 is "None" (lighting off)
        std::string label = o[0].to_str();
        size_t      dot   = label.find(". ");
        if (dot != std::string::npos && dot <= 3) label = label.substr(dot + 2);
        if ((int)b->names.size() < value) { b->names.resize(value); b->kinds.resize(value, 0xFF); }
        b->names[value - 1] = label;
        b->kinds[value - 1] = 0;
    }
    return true;
}

bool lookup_online(uint16_t vid, uint16_t pid, std::wstring *error) {
    std::string data;
    std::wstring path = L"/definitions/v3/" + std::to_wstring(((uint32_t)vid << 16) | pid) + L".json";
    if (!get(VIA_HOST, path, &data, error)) return false;
    // an unknown ID gets VIA's web page instead of a definition (M 2026-11-03: 3434:FFFF)
    if (!Json::parse(data).is(Json::OBJ)) { *error = L"VIA has no definition for this keyboard."; return false; }
    if (!write_file(via_path(vid, pid), data)) { *error = L"The definition could not be saved."; return false; }
    return true;
}

} // namespace boards
