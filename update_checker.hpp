#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <optional>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace aries {

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


class SimpleJson {
public:
    static std::string getString(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";

        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";

        pos++;

        while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;

        if (pos >= json.size() || json[pos] != '"')
            return "";

        pos++;

        std::string result;
        while (pos < json.size()) {
            char c = json[pos++];

            if (c == '\\') {
                if (pos >= json.size()) break;
                char esc = json[pos++];

                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += esc; break;
                }
            }
            else if (c == '"') {
                break;
            }
            else {
                result += c;
            }
        }

        return result;
    }

    static bool getBool(const std::string& json, const std::string& key, bool def = false) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return def;

        pos = json.find(':', pos);
        if (pos == std::string::npos) return def;

        pos++;

        while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;

        if (json.compare(pos, 4, "true") == 0) return true;
        if (json.compare(pos, 5, "false") == 0) return false;

        return def;
    }
};

// =========================
// HTTP 客户端（WinHTTP）
// =========================
class HttpClient {
public:
    static HttpResponse get(const std::wstring& host,
                            const std::wstring& path,
                            const std::string& token = "") {
        HttpResponse resp;

        HINTERNET hSession = WinHttpOpen(
            L"Aries-UpdateChecker/2.1",
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

        std::wstring headers =
            L"User-Agent: AriesUpdater\r\n"
            L"Accept: application/vnd.github+json\r\n"
            L"X-GitHub-Api-Version: 2022-11-28\r\n";

        if (!token.empty()) {
            headers += L"Authorization: Bearer " +
                std::wstring(token.begin(), token.end()) + L"\r\n";
        }

        if (!WinHttpSendRequest(
                hRequest,
                headers.c_str(),
                (DWORD)-1,
                WINHTTP_NO_REQUEST_DATA,
                0,
                0,
                0)) {
            cleanup(hSession, hConnect, hRequest);
            return resp;
        }

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
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0)
                break;

            std::string buffer;
            buffer.resize(dwSize);

            DWORD read = 0;
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &read) || read == 0)
                break;

            resp.body.append(buffer.data(), read);

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

// =========================
// 版本比较
// =========================
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
            auto dash = item.find('-');
            if (dash != std::string::npos)
                item = item.substr(0, dash);

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

// =========================
// 更新检查
// =========================
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

        r.version      = SimpleJson::getString(s, "tag_name");
        r.name         = SimpleJson::getString(s, "name");
        r.publishedAt  = SimpleJson::getString(s, "published_at");
        r.body         = SimpleJson::getString(s, "body");
        r.htmlUrl      = SimpleJson::getString(s, "html_url");
        r.isPrerelease = SimpleJson::getBool(s, "prerelease", false);

        r.success = !r.version.empty();

        if (!r.success) {
            r.errorMessage = "JSON parse failed or missing tag_name";
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
