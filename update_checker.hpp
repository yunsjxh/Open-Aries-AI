#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <optional>
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

namespace aries {

using json = nlohmann::json;

struct ReleaseInfo {
    std::string version;
    std::string name;
    std::string publishedAt;
    std::string body;
    std::string htmlUrl;
    bool isPrerelease = false;

    bool success = false;
    std::string errorMessage;
};

struct HttpResponse {
    std::string body;
    int statusCode = 0;
};

class HttpClient {
public:
    static HttpResponse get(const std::wstring& host,
                            const std::wstring& path,
                            const std::string& token = "") {
        HttpResponse resp;

        HINTERNET hSession = WinHttpOpen(
            L"Aries-UpdateChecker/2.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) return resp;

        HINTERNET hConnect = WinHttpConnect(
            hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);

        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return resp;
        }

        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"GET",
            path.c_str(),
            NULL,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE);

        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return resp;
        }

        // headers
        std::wstring headers = L"User-Agent: AriesUpdater\r\nAccept: application/vnd.github+json\r\n";

        if (!token.empty()) {
            headers += L"Authorization: Bearer " +
                       std::wstring(token.begin(), token.end()) + L"\r\n";
        }

        WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            (DWORD)-1,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0);

        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            cleanup(hSession, hConnect, hRequest);
            return resp;
        }

        DWORD status = 0;
        DWORD size = sizeof(status);

        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &status,
            &size,
            WINHTTP_NO_HEADER_INDEX);

        resp.statusCode = (int)status;

        if (status != 200) {
            cleanup(hSession, hConnect, hRequest);
            return resp;
        }

        DWORD dwSize = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (!dwSize) break;

            std::string buffer;
            buffer.resize(dwSize);

            DWORD read = 0;
            WinHttpReadData(hRequest, buffer.data(), dwSize, &read);
            resp.body.append(buffer, 0, read);

        } while (dwSize > 0);

        cleanup(hSession, hConnect, hRequest);
        return resp;
    }

private:
    static void cleanup(HINTERNET s, HINTERNET c, HINTERNET r) {
        if (r) WinHttpCloseHandle(r);
        if (c) WinHttpCloseHandle(c);
        if (s) WinHttpCloseHandle(s);
    }
};

class Version {
public:
    static std::vector<int> parse(const std::string& v) {
        std::vector<int> out;
        std::string s = v;

        if (!s.empty() && (s[0] == 'v' || s[0] == 'V'))
            s = s.substr(1);

        std::stringstream ss(s);
        std::string item;

        while (std::getline(ss, item, '.')) {
            try {
                out.push_back(std::stoi(item));
            } catch (...) {
                out.push_back(0);
            }
        }
        return out;
    }

    static int compare(const std::string& a, const std::string& b) {
        auto A = parse(a);
        auto B = parse(b);

        size_t n = std::max(A.size(), B.size());

        for (size_t i = 0; i < n; i++) {
            int x = i < A.size() ? A[i] : 0;
            int y = i < B.size() ? B[i] : 0;

            if (x < y) return -1;
            if (x > y) return 1;
        }
        return 0;
    }
};

class UpdateChecker {
public:
    static std::optional<ReleaseInfo> checkLatest(
        const std::string& repoUrl,
        const std::string& token = "") {

        auto [owner, repo] = parseRepo(repoUrl);
        if (owner.empty()) return std::nullopt;

        std::wstring host = L"api.github.com";
        std::wstring path = L"/repos/" +
            std::wstring(owner.begin(), owner.end()) +
            L"/" +
            std::wstring(repo.begin(), repo.end()) +
            L"/releases/latest";

        auto resp = HttpClient::get(host, path, token);

        if (resp.statusCode != 200 || resp.body.empty())
            return std::nullopt;

        return parse(resp.body);
    }

private:
    static ReleaseInfo parse(const std::string& s) {
        ReleaseInfo r;

        try {
            auto j = json::parse(s);

            r.version = j.value("tag_name", "");
            r.name = j.value("name", "");
            r.publishedAt = j.value("published_at", "");
            r.body = j.value("body", "");
            r.htmlUrl = j.value("html_url", "");
            r.isPrerelease = j.value("prerelease", false);

            r.success = !r.version.empty();

        } catch (const std::exception& e) {
            r.errorMessage = e.what();
        }

        return r;
    }

    static std::pair<std::string, std::string> parseRepo(const std::string& url) {
        std::string u = url;

        auto pos = u.find("github.com/");
        if (pos == std::string::npos)
            return {};

        u = u.substr(pos + 11);

        if (u.ends_with("/"))
            u.pop_back();

        if (u.ends_with(".git"))
            u = u.substr(0, u.size() - 4);

        auto slash = u.find('/');
        if (slash == std::string::npos)
            return {};

        return {u.substr(0, slash), u.substr(slash + 1)};
    }
};

} // namespace aries
