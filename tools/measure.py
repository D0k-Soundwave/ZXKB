"""Measure a running program's RAM and CPU, e.g. ZXKB idle or busy.

    python tools/measure.py ZXKB.exe 60

Reports, summed over every process with that name:
  - working set: all RAM pages the program touches, including shared Windows DLL pages
  - private working set: RAM only this program uses (the honest "how much does it cost" figure)
  - private bytes (commit): memory reserved for it, in RAM or the page file
  - CPU: average over the sample period, as % of one core and % of the whole machine
Uses the Windows API directly (no extra packages), so it works when performance counters are broken.
"""
import ctypes, ctypes.wintypes as w, sys, time

k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)

class PMC2(ctypes.Structure):  # PROCESS_MEMORY_COUNTERS_EX2 (Windows 10 21H2+)
    _fields_ = [("cb", w.DWORD), ("PageFaultCount", w.DWORD), ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t), ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t), ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t), ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t), ("PrivateUsage", ctypes.c_size_t),
                ("PrivateWorkingSetSize", ctypes.c_size_t), ("SharedCommitUsage", ctypes.c_ulonglong)]

class PE32(ctypes.Structure):
    _fields_ = [("dwSize", w.DWORD), ("cntUsage", w.DWORD), ("th32ProcessID", w.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", w.DWORD), ("cntThreads", w.DWORD),
                ("th32ParentProcessID", w.DWORD), ("pcPriClassBase", w.LONG), ("dwFlags", w.DWORD),
                ("szExeFile", w.WCHAR * 260)]

k32.CreateToolhelp32Snapshot.restype = w.HANDLE
k32.OpenProcess.restype = w.HANDLE
psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, ctypes.c_void_p, w.DWORD]
k32.GetProcessTimes.argtypes = [w.HANDLE] + [ctypes.POINTER(w.FILETIME)] * 4

def find(name):
    snap = k32.CreateToolhelp32Snapshot(2, 0)
    e = PE32(dwSize=ctypes.sizeof(PE32)); out = []
    ok = k32.Process32FirstW(snap, ctypes.byref(e))
    while ok:
        if e.szExeFile.lower() == name.lower(): out.append((e.th32ProcessID, e.cntThreads))
        ok = k32.Process32NextW(snap, ctypes.byref(e))
    k32.CloseHandle(snap)
    return out

def cpu_seconds(h):
    c, x, kt, ut = w.FILETIME(), w.FILETIME(), w.FILETIME(), w.FILETIME()
    k32.GetProcessTimes(h, ctypes.byref(c), ctypes.byref(x), ctypes.byref(kt), ctypes.byref(ut))
    f = lambda t: (t.dwHighDateTime << 32 | t.dwLowDateTime) / 1e7
    return f(kt) + f(ut)

def main():
    name = sys.argv[1]; secs = float(sys.argv[2]) if len(sys.argv) > 2 else 30
    procs = find(name)
    if not procs: sys.exit(f"{name} is not running")
    handles = [k32.OpenProcess(0x1000 | 0x0010, False, pid) for pid, _ in procs]  # query limited info + vm read
    t0 = time.perf_counter(); c0 = sum(cpu_seconds(h) for h in handles)
    time.sleep(secs)
    c1 = sum(cpu_seconds(h) for h in handles); t1 = time.perf_counter()
    ws = pws = priv = 0
    for h in handles:
        m = PMC2(cb=ctypes.sizeof(PMC2))
        psapi.GetProcessMemoryInfo(h, ctypes.byref(m), m.cb)
        ws += m.WorkingSetSize; pws += m.PrivateWorkingSetSize; priv += m.PrivateUsage
        k32.CloseHandle(h)
    cores = __import__("os").cpu_count()
    one = (c1 - c0) / (t1 - t0) * 100
    MB = 1024 * 1024
    print(f"{name}: {len(procs)} process(es), {sum(t for _, t in procs)} threads, sampled {t1 - t0:.0f} s on {time.strftime('%Y-%m-%d %H:%M')}")
    print(f"  working set {ws / MB:.1f} MB | private working set {pws / MB:.1f} MB | private bytes {priv / MB:.1f} MB")
    print(f"  CPU {one:.3f}% of one core | {one / cores:.4f}% of the machine ({cores} logical CPUs)")

main()
