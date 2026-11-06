// Firmware updates from the ZXKB window. The keyboard restarts into its bootloader when ZXKB asks (firmware v17 on,
// settings kept), or the user holds Esc while plugging it in (older firmware; that also clears its settings).
// dfu-util (GPL, shipped next to ZXKB.exe) then writes the chosen .bin. Only the STM32 ROM bootloader (USB 0483:DF11)
// is used; any other DFU device on the PC is never touched.
// Copyright 2026 D0k
// SPDX-License-Identifier: GPL-3.0-or-later
#include "flash.h"
#include "app.h"
#include "settings.h"
#include "protocol.h"
#include "../res/resource.h"
#include "layoutcheck.h"
#include "keystore.h"
#include <commctrl.h>
#include <commdlg.h>
#include <setupapi.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>

namespace {

constexpr UINT WM_FW_STATUS   = WM_APP + 100; // lParam: new std::wstring, deleted here
constexpr UINT WM_FW_PROGRESS = WM_APP + 101; // wParam: 0-100
constexpr UINT WM_FW_DONE     = WM_APP + 102; // wParam: 1 written
constexpr UINT WM_FW_CHECK    = WM_APP + 103; // the keyboard is back: check its layout against the copy
std::wstring   g_backup_file; // the layout copied before this install
constexpr UINT TIMER_RECONNECT = 1;
// [A] waits. Not measured yet: how long the Q3 takes to show up as a DFU device after the command, and to start
// again after writing. QMK Toolbox writes the Q3's ~60 KB in a few seconds (seen by D0k, not timed).
constexpr DWORD BOOT_WAIT_MS   = 10000;  // after the bootloader command
constexpr DWORD MANUAL_WAIT_MS = 120000; // for the user to unplug, hold Esc and plug in
constexpr DWORD WRITE_LIMIT_MS = 120000; // dfu-util is stopped if it takes longer
constexpr UINT  RECONNECT_MS   = 20000;  // for the keyboard to start again
constexpr DWORD MAX_FILE       = 2 * 1024 * 1024; // [P] largest STM32 flash QMK supports is 2 MB (STM32F4/H7 class)

enum State { IDLE, WAITING, WRITING, RECONNECTING };
HWND               g_dlg   = nullptr;
volatile State     g_state = IDLE;
volatile bool      g_cancel = false;
DWORD              g_wait_ms = 0;
std::wstring       g_folder, g_file;
std::vector<std::wstring> g_files;

std::wstring exe_dir() {
    wchar_t p[MAX_PATH];
    DWORD   n = GetModuleFileNameW(nullptr, p, MAX_PATH);
    std::wstring s(p, n);
    return s.substr(0, s.find_last_of(L'\\'));
}

void post_status(const std::wstring &s) { PostMessageW(g_dlg, WM_FW_STATUS, 0, (LPARAM) new std::wstring(s)); }
void set_status(const std::wstring &s) { SetDlgItemTextW(g_dlg, IDC_FW_STATUS, s.c_str()); }

int version_of(const std::wstring &name) { // "ZXKB_Q3_v17.bin" -> 17, 0 if none
    size_t v = name.rfind(L"_v");
    return v == std::wstring::npos ? 0 : _wtoi(name.c_str() + v + 2);
}

// Is the STM32 bootloader on USB now?
bool dfu_present() {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"USB", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    SP_DEVINFO_DATA d = {sizeof(d)};
    for (DWORD i = 0; !found && SetupDiEnumDeviceInfo(set, i, &d); i++) {
        wchar_t ids[1024] = {0};
        if (!SetupDiGetDeviceRegistryPropertyW(set, &d, SPDRP_HARDWAREID, nullptr, (BYTE *)ids, sizeof(ids) - 4, nullptr)) continue;
        for (wchar_t *id = ids; *id && !found; id += wcslen(id) + 1) {
            CharUpperW(id);
            found = wcsstr(id, L"VID_0483&PID_DF11") != nullptr;
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

void log_bytes(const char *s, DWORD n);
// A timed line in firmware.log, to learn how long each phase really takes
DWORD g_t0;
void log_phase(const char *what) {
    char line[160];
    SYSTEMTIME t;
    GetLocalTime(&t);
    int n = wsprintfA(line, "\r\n[%02d:%02d:%02d, +%lu ms] %s\r\n", t.wHour, t.wMinute, t.wSecond, GetTickCount() - g_t0, what);
    log_bytes(line, (DWORD)n);
}

void log_bytes(const char *s, DWORD n) {
    HANDLE f = CreateFileW((settings::folder() + L"\\firmware.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD w;
    WriteFile(f, s, n, &w, nullptr);
    CloseHandle(f);
}

// One line of dfu-util output: progress and the phase it is in
void dfu_line(const std::string &line, std::string *last) {
    if (line.empty()) return;
    *last = line;
    size_t pc = line.find('%');
    bool erase = line.rfind("Erase", 0) == 0, write = line.rfind("Download", 0) == 0;
    if (pc != std::string::npos && (erase || write)) {
        size_t b = pc;
        while (b > 0 && line[b - 1] >= '0' && line[b - 1] <= '9') b--;
        int p = atoi(line.c_str() + b);
        PostMessageW(g_dlg, WM_FW_PROGRESS, erase ? p / 4 : 25 + p * 3 / 4, 0); // [A] erasing is the shorter part
    }
}

DWORD WINAPI work(void *) {
    // 1. wait for the bootloader
    DWORD start = GetTickCount();
    log_phase("waiting for the bootloader");
    while (!dfu_present()) {
        if (g_cancel) { PostMessageW(g_dlg, WM_FW_DONE, 0, (LPARAM) new std::wstring(L"Cancelled. Nothing was written.")); return 0; }
        if (GetTickCount() - start > g_wait_ms) {
            PostMessageW(g_dlg, WM_FW_DONE, 0, (LPARAM) new std::wstring(
                L"The keyboard's bootloader did not appear, so nothing was written. Unplug the keyboard, hold Esc while "
                L"plugging it back in, then press Install again (holding Esc also clears the keyboard's settings)."));
            return 0;
        }
        Sleep(250);
    }
    g_state = WRITING;
    log_phase("bootloader found");
    post_status(L"Writing the firmware. Do not unplug the keyboard...");

    // 2. dfu-util writes the file and starts the new firmware (":leave")
    std::wstring tool = exe_dir() + L"\\dfu-util.exe";
    std::wstring cmd  = L"\"" + tool + L"\" -a 0 -d 0483:DF11 -s 0x08000000:leave -D \"" + g_file + L"\"";
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE rd, wr;
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si = {};
    si.cb         = sizeof(si);
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError  = wr;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi;
    std::string head = "\r\n--- " + settings::narrow(cmd) + "\r\n";
    log_bytes(head.c_str(), (DWORD)head.size());
    if (!CreateProcessW(tool.c_str(), &cmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(rd);
        CloseHandle(wr);
        PostMessageW(g_dlg, WM_FW_DONE, 0, (LPARAM) new std::wstring(L"dfu-util.exe was not found next to ZXKB.exe, so nothing was written."));
        return 0;
    }
    CloseHandle(wr);
    std::string line, last, all;
    char buf[512];
    DWORD n;
    while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) { // ends when dfu-util exits
        log_bytes(buf, n);
        all.append(buf, n);
        for (DWORD i = 0; i < n; i++) {
            if (buf[i] == '\r' || buf[i] == '\n') { dfu_line(line, &last); line.clear(); }
            else line += buf[i];
        }
    }
    dfu_line(line, &last);
    CloseHandle(rd);
    if (WaitForSingleObject(pi.hProcess, WRITE_LIMIT_MS) == WAIT_TIMEOUT) TerminateProcess(pi.hProcess, 1);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    log_phase(code == 0 ? "dfu-util finished" : "dfu-util failed");
    bool ok = all.find("File downloaded successfully") != std::string::npos || (code == 0 && all.find("Download done") != std::string::npos);
    PostMessageW(g_dlg, WM_FW_DONE, ok, ok ? 0 : (LPARAM) new std::wstring(
        L"Writing failed (" + settings::widen(last) + L"). The keyboard stays in its bootloader with its lights off until "
        L"firmware is written: press Install again, or choose another file. Details are in firmware.log."));
    return 0;
}

void fill_list() {
    HWND list = GetDlgItem(g_dlg, IDC_FW_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    g_files.clear();
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW((g_folder + L"\\*.bin").c_str(), &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do g_files.push_back(fd.cFileName);
        while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    std::sort(g_files.begin(), g_files.end(), [](const std::wstring &a, const std::wstring &b) {
        int va = version_of(a), vb = version_of(b);
        return va != vb ? va > vb : a < b; // newest first
    });
    for (const std::wstring &name : g_files) {
        std::wstring text = name;
        if (g_board && g_st.fw_version && version_of(name) == g_st.fw_version) text += L"  (on the keyboard now)";
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)text.c_str());
    }
    SendMessageW(list, LB_SETCURSEL, g_files.empty() ? -1 : 0, 0);
    SetDlgItemTextW(g_dlg, IDC_FW_FOLDER, (L"Firmware files in " + g_folder).c_str());
}

void show_keyboard() {
    std::wstring s = L"Keyboard: not connected";
    if (g_board) {
        s = L"Keyboard: " + g_board->product;
        if (g_st.fw_version) s += L", firmware v" + std::to_wstring(g_st.fw_version);
    }
    SetDlgItemTextW(g_dlg, IDC_FW_INFO, s.c_str());
}

void set_busy(bool busy, bool can_cancel) {
    EnableWindow(GetDlgItem(g_dlg, IDC_FW_INSTALL), !busy);
    EnableWindow(GetDlgItem(g_dlg, IDC_FW_LIST), !busy);
    EnableWindow(GetDlgItem(g_dlg, IDC_FW_BROWSE), !busy);
    EnableWindow(GetDlgItem(g_dlg, IDCANCEL), !busy || can_cancel);
    SetDlgItemTextW(g_dlg, IDCANCEL, busy ? L"Cancel" : L"Close");
}

// The chosen file must be firmware for this keyboard: its USB name is stored in the file (UTF-16, as USB sends it)
// Is this text in the file? ("PC_DIRECT", the name of ZXKB's direct-colour effect, marks a ZXKB build)
bool file_has(const std::wstring &path, const char *text) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD size = GetFileSize(f, nullptr), n = 0;
    std::vector<char> data(size <= MAX_FILE ? size : 0);
    bool ok = size <= MAX_FILE && ReadFile(f, data.data(), size, &n, nullptr) && n == size;
    CloseHandle(f);
    return ok && std::search(data.begin(), data.end(), text, text + strlen(text)) != data.end();
}

bool check_file(const std::wstring &path, std::wstring *why) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) { *why = L"The file could not be opened."; return false; }
    DWORD size = GetFileSize(f, nullptr), n = 0;
    std::vector<char> data(size <= MAX_FILE ? size : 0);
    bool read = size > 1024 && size <= MAX_FILE && ReadFile(f, data.data(), size, &n, nullptr) && n == size;
    CloseHandle(f);
    if (!read) { *why = L"This is not a keyboard firmware file (wrong size)."; return false; }
    // Every QMK firmware holds its USB device descriptor: 0x12 0x01, USB version, class, subclass, protocol, packet
    // size, then vendor and product ID. The IDs must be this keyboard's. They stay the same across firmwares for the
    // same keyboard (M 2026-11-03: ZXKB v17 and Keychron's own Q3 build both 3434:0123; the Q3 HE file 3434:0B31),
    // while the USB name can differ ("keychron/q3/iso_encoder" in ZXKB builds, "Keychron Q3" in Keychron's).
    unsigned want = g_board ? ((unsigned)g_board->vid << 16 | g_board->pid) : (unsigned)settings::get_int(L"firmware", L"usb", 0);
    if (!want) return true; // never connected: nothing to compare with
    std::vector<unsigned> found;
    for (DWORD i = 0; i + 12 <= size; i++) {
        const uint8_t *d = reinterpret_cast<const uint8_t *>(data.data()) + i;
        if (d[0] == 0x12 && d[1] == 0x01 && d[2] <= 0x03 && d[3] == 0x02) found.push_back((unsigned)(d[8] | d[9] << 8) << 16 | (d[10] | d[11] << 8));
    }
    for (unsigned id : found)
        if (id == want) return true;
    wchar_t ids[64];
    swprintf(ids, 64, found.empty() ? L"no USB ID found in it" : L"it is for USB ID %04X:%04X", found.empty() ? 0 : found[0] >> 16,
             found.empty() ? 0 : found[0] & 0xFFFF);
    wchar_t mine[16];
    swprintf(mine, 16, L"%04X:%04X", want >> 16, want & 0xFFFF);
    *why = std::wstring(L"This firmware is not for this keyboard (") + mine + L"): " + ids + L".";
    return false;
}

void install() {
    int sel = (int)SendDlgItemMessageW(g_dlg, IDC_FW_LIST, LB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= (int)g_files.size()) return;
    g_file = g_folder + L"\\" + g_files[sel];
    std::wstring why;
    if (!check_file(g_file, &why)) { MessageBoxW(g_dlg, why.c_str(), L"ZXKB", MB_OK | MB_ICONWARNING); return; }

    bool in_dfu = dfu_present();
    bool by_command = !in_dfu && g_board && g_board->protocol >= 4 && g_st.bootloader == proto::BOOT_STM32_DFU;
    if (!in_dfu && g_board && g_board->protocol >= 4 && g_st.bootloader != proto::BOOT_STM32_DFU) {
        MessageBoxW(g_dlg, L"ZXKB can only install firmware on keyboards with the STM32 bootloader so far.", L"ZXKB", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring ask = L"Install " + g_files[sel] + L" on the keyboard?\n\n";
    // a ZXKB build keeps the keyboard's settings; another kind of firmware may reset them on its first start
    bool zxkb_file = file_has(g_file, "PC_DIRECT");
    ask += by_command || in_dfu ? (zxkb_file ? L"The keyboard stops working for a few seconds while it is written. Its settings are kept."
                                             : L"The keyboard stops working for a few seconds while it is written. This is not ZXKB "
                                               L"firmware, so the keyboard may reset its settings when it starts.")
                                : L"This keyboard's firmware cannot restart itself into its bootloader, so you will be asked to "
                                  L"unplug it and plug it back in while holding Esc. Holding Esc also clears its settings.";
    if (MessageBoxW(g_dlg, ask.c_str(), L"ZXKB", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    // copy the whole key layout first, to check and put back after (D0k, 2026-11-06)
    g_backup_file.clear();
    if (!in_dfu && layoutcheck::possible()) {
        set_status(L"Copying the key layout, knob and macros...");
        std::wstring e;
        g_backup_file = layoutcheck::backup(L"before installing " + g_files[sel], &e);
        if (g_backup_file.empty() &&
            MessageBoxW(g_dlg, (e + L"\n\nInstall anyway, without a copy to check against?").c_str(), L"ZXKB", MB_YESNO | MB_ICONWARNING) != IDYES) {
            set_status(L"");
            return;
        }
        if (!g_backup_file.empty()) keystore::hold_reapply(true);
    }

    g_cancel = false;
    g_state  = WAITING;
    g_t0     = GetTickCount();
    log_phase(settings::narrow(L"install " + g_files[sel] + (by_command ? L" (bootloader command)" : L" (by hand)")).c_str());
    SendDlgItemMessageW(g_dlg, IDC_FW_PROGRESS, PBM_SETPOS, 0, 0);
    if (by_command) {
        g_wait_ms = BOOT_WAIT_MS;
        set_status(L"Restarting the keyboard into its bootloader...");
        enter_bootloader();
    } else {
        g_wait_ms = MANUAL_WAIT_MS;
        set_status(in_dfu ? L"The keyboard is in its bootloader." : L"Unplug the keyboard, then hold Esc while plugging it back in.");
    }
    set_busy(true, !by_command);
    if (HANDLE t = CreateThread(nullptr, 64 * 1024, work, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr)) CloseHandle(t);
}

void browse() {
    wchar_t path[MAX_PATH] = {0};
    OPENFILENAMEW of = {sizeof(of)};
    of.hwndOwner       = g_dlg;
    of.lpstrFilter     = L"Keyboard firmware (*.bin)\0*.bin\0";
    of.lpstrFile       = path;
    of.nMaxFile        = MAX_PATH;
    of.lpstrInitialDir = g_folder.c_str();
    of.Flags           = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&of)) return;
    std::wstring p = path;
    g_folder = p.substr(0, p.find_last_of(L'\\'));
    settings::set(L"firmware", L"folder", g_folder);
    fill_list();
    std::wstring name = p.substr(p.find_last_of(L'\\') + 1);
    for (size_t i = 0; i < g_files.size(); i++)
        if (g_files[i] == name) SendDlgItemMessageW(g_dlg, IDC_FW_LIST, LB_SETCURSEL, i, 0);
}

INT_PTR CALLBACK proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_INITDIALOG:
            g_dlg    = dlg;
            g_state  = IDLE;
            g_folder = settings::get(L"firmware", L"folder", (exe_dir() + L"\\firmware").c_str());
            SendDlgItemMessageW(dlg, IDC_FW_PROGRESS, PBM_SETRANGE32, 0, 100);
            show_keyboard();
            fill_list();
            set_status(L"");
            return TRUE;
        case WM_FW_STATUS: {
            auto *s = reinterpret_cast<std::wstring *>(lp);
            set_status(*s);
            delete s;
            set_busy(true, false); // writing: no cancelling
            return TRUE;
        }
        case WM_FW_CHECK:
            if (!g_backup_file.empty()) {
                Sleep(500); // [A] let the tray copy finish its own start-up talk with the keyboard first
                layoutcheck::check(dlg, g_backup_file, true);
                keystore::hold_reapply(false);
                g_backup_file.clear();
                set_status(L"Done: the keyboard runs firmware v" + std::to_wstring(g_st.fw_version) + L". Key layout checked.");
            }
            return TRUE;
        case WM_FW_PROGRESS:
            SendDlgItemMessageW(dlg, IDC_FW_PROGRESS, PBM_SETPOS, wp, 0);
            return TRUE;
        case WM_FW_DONE:
            if (wp) {
                g_state = RECONNECTING;
                SendDlgItemMessageW(dlg, IDC_FW_PROGRESS, PBM_SETPOS, 100, 0);
                set_status(L"Firmware written. Waiting for the keyboard to start...");
                SetTimer(dlg, TIMER_RECONNECT, RECONNECT_MS, nullptr);
            } else {
                auto *s = reinterpret_cast<std::wstring *>(lp);
                g_state = IDLE;
                set_status(*s);
                delete s;
                set_busy(false, false);
            }
            return TRUE;
        case WM_TIMER:
            if (wp == TIMER_RECONNECT) {
                KillTimer(dlg, TIMER_RECONNECT);
                if (g_state == RECONNECTING) {
                    g_state = IDLE;
                    // [M] 2026-11-03: once in two installs the Q3 stayed dark after writing until it was replugged
                    log_phase(dfu_present() ? "keyboard not back: still in its bootloader" : "keyboard not back: not on USB");
                    set_status(L"The firmware is written and checked, but the keyboard did not restart by itself (this happens "
                               L"now and then). Unplug the keyboard and plug it back in: it will start with the new firmware.");
                    set_busy(false, false);
                }
            }
            return TRUE;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDC_FW_INSTALL: install(); return TRUE;
                case IDC_FW_BROWSE: browse(); return TRUE;
                case IDC_FW_COPYNOW: { // a copy of the layout, to put back later
                    if (g_state != IDLE) return TRUE;
                    std::wstring e, f = layoutcheck::possible() ? layoutcheck::backup(L"copied by hand", &e) : L"";
                    if (f.empty()) MessageBoxW(dlg, e.empty() ? L"This keyboard's key layout cannot be read." : e.c_str(), L"ZXKB", MB_OK | MB_ICONWARNING);
                    else set_status(L"Copied: " + f.substr(f.find_last_of(L'\\') + 1));
                    return TRUE;
                }
                case IDC_FW_BACKUPS: // put back any earlier layout copy
                    if (g_state == IDLE) layoutcheck::choose_and_check(dlg);
                    return TRUE;
                case IDC_FW_LIST:
                    if (HIWORD(wp) == LBN_DBLCLK) install();
                    return TRUE;
                case IDCANCEL:
                    if (g_state == WAITING) { g_cancel = true; return TRUE; } // the thread reports back
                    if (g_state == WRITING) return TRUE;                      // never stop half way
                    KillTimer(dlg, TIMER_RECONNECT);
                    g_dlg = nullptr;
                    g_state = IDLE;
                    EndDialog(dlg, 0);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

} // namespace

void flash_dialog(HWND owner) {
    if (g_dlg) { SetForegroundWindow(g_dlg); return; }
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_FIRMWARE), owner, proc, 0);
}

void flash_refresh() {
    if (g_board && settings::get_int(L"firmware", L"usb", 0) != (int)((unsigned)g_board->vid << 16 | g_board->pid))
        settings::set_int(L"firmware", L"usb", (int)((unsigned)g_board->vid << 16 | g_board->pid)); // to check files while it is in its bootloader
    if (!g_dlg) return;
    show_keyboard();
    if (g_state == IDLE) fill_list();
    if (g_state == RECONNECTING && g_board) {
        KillTimer(g_dlg, TIMER_RECONNECT);
        g_state = IDLE;
        fill_list();
        log_phase("keyboard back");
        set_status(L"Done: the keyboard runs firmware v" + std::to_wstring(g_st.fw_version) + L"." +
                   (g_backup_file.empty() ? L"" : L" Checking the key layout..."));
        set_busy(false, false);
        if (!g_backup_file.empty()) PostMessageW(g_dlg, WM_FW_CHECK, 0, 0); // after the connection is set up
    }
}

bool flash_busy() { return g_state == WAITING || g_state == WRITING; }
