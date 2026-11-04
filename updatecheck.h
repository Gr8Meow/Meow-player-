// updatecheck.h - tiny, safe "update available" check for GitHub releases.
// Developed by Gr8Meow. MIT license.
//
// How it works:
//   * One HTTPS request (WinHTTP, certificate checks on, TLS 1.2+) to
//     https://github.com/<owner>/<repo>/releases/latest
//   * GitHub answers with a redirect to .../releases/tag/<tag>; the tag is the latest
//     published (non-draft, non-prerelease) release. Nothing is downloaded.
//   * The tag is compared with the running version. The app only shows a notice and
//     opens the release page in the browser when the user clicks it.
//   * No personal data is sent: just a normal HTTPS request from the user's IP address
//     with a User-Agent such as "Meow Player update check".
//   * Runs on a background thread, 8 second timeouts, never blocks the UI.
//
// Usage:
//   UC_Start(hwnd, WM_MY_UPDATE, "owner/repo", "1.0.0", L"Meow Player");
//   // in the window procedure, on WM_MY_UPDATE:
//   UpdateResult r; if (UC_Take(r)) { if (r.ok && r.newer) { ... r.tag ... } }
#pragma once

#include <windows.h>
#include <winhttp.h>

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif

struct UpdateResult {
    bool ok = false;     // the check itself worked
    bool newer = false;  // the latest release is newer than the running version
    std::string tag;     // e.g. "v1.2.0"
};

// ---- validation helpers (the repo name and the tag end up in URLs) ----
static bool UC_ValidTag(const std::string& t) {
    if (t.empty() || t.size() > 64) return false;
    for (char c : t) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' ||
                  c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

static bool UC_ValidRepo(const std::string& r) {
    size_t slash = r.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= r.size() || r.size() > 120) return false;
    std::string owner = r.substr(0, slash), name = r.substr(slash + 1);
    if (name.find('/') != std::string::npos || name == "." || name == "..") return false;
    for (char c : owner) {  // GitHub user names: letters, digits, hyphens
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-')) return false;
    }
    for (char c : name) {  // repository names: letters, digits, . _ -
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '_' ||
              c == '-'))
            return false;
    }
    return true;
}

// "v1.10.2-beta" -> {1, 10, 2}
static std::vector<int> UC_Parse(const std::string& v) {
    std::vector<int> parts;
    size_t i = 0;
    if (i < v.size() && (v[i] == 'v' || v[i] == 'V')) i++;
    int cur = 0;
    bool have = false;
    for (; i < v.size(); i++) {
        char c = v[i];
        if (c >= '0' && c <= '9') {
            if (cur < 100000000) cur = cur * 10 + (c - '0');
            have = true;
        } else if (c == '.' && have) {
            parts.push_back(cur);
            cur = 0;
            have = false;
        } else {
            break;
        }
    }
    if (have) parts.push_back(cur);
    return parts;
}

// >0 if a is newer than b, <0 if older, 0 if equal
static int UC_Compare(const std::string& a, const std::string& b) {
    std::vector<int> x = UC_Parse(a), y = UC_Parse(b);
    size_t n = x.size() > y.size() ? x.size() : y.size();
    for (size_t i = 0; i < n; i++) {
        int xi = i < x.size() ? x[i] : 0, yi = i < y.size() ? y[i] : 0;
        if (xi != yi) return xi > yi ? 1 : -1;
    }
    return 0;
}

// ---- network ----
static bool UC_FetchLatestTag(const std::string& repo, const std::wstring& agent, std::string& tagOut) {
    if (!UC_ValidRepo(repo)) return false;
    std::wstring path = L"/" + std::wstring(repo.begin(), repo.end()) + L"/releases/latest";
    bool ok = false;

    HINTERNET ses = WinHttpOpen((agent + L" update check").c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return false;
    WinHttpSetTimeouts(ses, 8000, 8000, 8000, 8000);
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | 0x00002000;  // TLS 1.2 + 1.3
    if (!WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols))) {
        protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
        WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));
    }

    HINTERNET con = WinHttpConnect(ses, L"github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                        : nullptr;
    if (req) {
        DWORD noRedirect = WINHTTP_DISABLE_REDIRECTS;  // we only want the Location header
        WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &noRedirect, sizeof(noRedirect));
        if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, nullptr)) {
            DWORD status = 0, len = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                wchar_t loc[1024] = {0};
                DWORD llen = sizeof(loc) - sizeof(wchar_t);
                if (WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, loc, &llen,
                                        WINHTTP_NO_HEADER_INDEX)) {
                    std::wstring l = loc;
                    bool hostOk = l.rfind(L"https://github.com/", 0) == 0 || l.rfind(L"/", 0) == 0;
                    size_t at = l.find(L"/releases/tag/");
                    if (hostOk && at != std::wstring::npos) {
                        std::wstring t = l.substr(at + 14);
                        size_t cut = t.find_first_of(L"?#/");
                        if (cut != std::wstring::npos) t = t.substr(0, cut);
                        std::string tag;
                        for (wchar_t c : t) tag += c < 128 ? (char)c : '?';
                        if (UC_ValidTag(tag)) {
                            tagOut = tag;
                            ok = true;
                        }
                    }
                }
            }
        }
        WinHttpCloseHandle(req);
    }
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

// ---- background thread + hand-off (no pointers travel through window messages) ----
static CRITICAL_SECTION UC_cs;
static bool UC_csInit = false;
static UpdateResult UC_res;
static bool UC_has = false;
static volatile LONG UC_running = 0;

struct UC_Job {
    HWND hwnd;
    UINT msg;
    std::string repo, current;
    std::wstring agent;
};

static DWORD WINAPI UC_Thread(LPVOID param) {
    UC_Job* job = static_cast<UC_Job*>(param);
    UpdateResult r;
    std::string tag;
    if (UC_FetchLatestTag(job->repo, job->agent, tag)) {
        r.ok = true;
        r.tag = tag;
        r.newer = UC_Compare(tag, job->current) > 0;
    }
    EnterCriticalSection(&UC_cs);
    UC_res = r;
    UC_has = true;
    LeaveCriticalSection(&UC_cs);
    PostMessageW(job->hwnd, job->msg, 0, 0);
    delete job;
    InterlockedExchange(&UC_running, 0);
    return 0;
}

// Returns false if a check is already running or the thread could not start.
static bool UC_Start(HWND hwnd, UINT msg, const std::string& repo, const std::string& current,
                     const std::wstring& agent) {
    if (!UC_csInit) {
        InitializeCriticalSection(&UC_cs);
        UC_csInit = true;
    }
    if (InterlockedCompareExchange(&UC_running, 1, 0) != 0) return false;
    UC_Job* job = new UC_Job{hwnd, msg, repo, current, agent};
    HANDLE h = CreateThread(nullptr, 0, UC_Thread, job, 0, nullptr);
    if (!h) {
        delete job;
        InterlockedExchange(&UC_running, 0);
        return false;
    }
    CloseHandle(h);
    return true;
}

// Fetches the finished result (once). Returns false if there is none.
static bool UC_Take(UpdateResult& out) {
    if (!UC_csInit) return false;
    bool got = false;
    EnterCriticalSection(&UC_cs);
    if (UC_has) {
        out = UC_res;
        UC_has = false;
        got = true;
    }
    LeaveCriticalSection(&UC_cs);
    return got;
}
