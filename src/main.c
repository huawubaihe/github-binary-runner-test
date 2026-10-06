#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>

#define OWNER L"huawubaihe"
#define REPO L"github-binary-runner-test"
#define LIMIT (16UL * 1024 * 1024)

static HINTERNET session;

static HINTERNET get(const wchar_t *host, const wchar_t *path, HINTERNET *connection) {
    HINTERNET request;
    DWORD status, length = sizeof(status);
    *connection = WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!*connection) return NULL;
    request = WinHttpOpenRequest(*connection, L"GET", path, NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!request) return NULL;
    if (!WinHttpSendRequest(request, NULL, 0, NULL, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, NULL) ||
        !WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             NULL, &status, &length, NULL)) {
        WinHttpCloseHandle(request);
        return NULL;
    }
    if (status != 200) {
        fwprintf(stderr, L"请求失败，HTTP 状态码：%lu。\n", status);
        WinHttpCloseHandle(request);
        return NULL;
    }
    return request;
}

static char *latest(void) {
    HINTERNET connection = NULL, request;
    char *data = NULL, *next;
    DWORD used = 0, count;
    unsigned char buffer[8192];
    request = get(L"api.github.com", L"/repos/" OWNER L"/" REPO L"/releases/latest", &connection);
    if (!request) goto failed;
    for (;;) {
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &count)) goto failed;
        if (!count) break;
        if (count > LIMIT - used) goto failed;
        next = (char *)realloc(data, (size_t)used + count + 1);
        if (!next) goto failed;
        data = next;
        memcpy(data + used, buffer, count);
        used += count;
        data[used] = 0;
    }
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    return data;
failed:
    free(data);
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    return NULL;
}

/* GitHub 返回的下载地址只包含 ASCII；仅提取精确 JSON 字段。 */
static int next_url(const char **cursor, wchar_t url[2048]) {
    const char *p = *cursor;
    char value[2048];
    size_t n;
    while (*p) {
        char key[128];
        size_t k = 0;
        if (*p++ != '"') continue;
        while (*p && *p != '"') {
            char c = *p++;
            if (c == '\\' && *p) c = *p++;
            if (k + 1 < sizeof(key)) key[k++] = c;
        }
        if (!*p) break;
        ++p;
        key[k] = 0;
        if (strcmp(key, "browser_download_url")) continue;
        while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') ++p;
        if (*p++ != ':') break;
        while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') ++p;
        if (*p++ != '"') break;
        n = 0;
        while (*p && *p != '"') {
            char c = *p++;
            if (c == '\\') {
                if (*p != '/' && *p != '\\' && *p != '"') return -1;
                c = *p++;
            }
            if (n + 1 >= sizeof(value)) return -1;
            value[n++] = c;
        }
        if (*p != '"') return -1;
        value[n] = 0;
        *cursor = p + 1;
        return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, url, 2048) ? 1 : -1;
    }
    *cursor = p;
    return 0;
}

static int executable(const unsigned char *header, DWORD size) {
    DWORD offset;
    WORD machine, flags, magic, optional_size, subsystem;
    SYSTEM_INFO system;
    if (size < 94 || memcmp(header, "MZ", 2)) return 0;
    memcpy(&offset, header + 60, 4);
    if (offset > size - 94 || memcmp(header + offset, "PE\0\0", 4)) return 0;
    memcpy(&machine, header + offset + 4, 2);
    memcpy(&optional_size, header + offset + 20, 2);
    memcpy(&flags, header + offset + 22, 2);
    memcpy(&magic, header + offset + 24, 2);
    memcpy(&subsystem, header + offset + 92, 2);
    if (optional_size < 70 || !(flags & IMAGE_FILE_EXECUTABLE_IMAGE) ||
        (flags & IMAGE_FILE_DLL) || (subsystem != 2 && subsystem != 3)) return 0;
    GetNativeSystemInfo(&system);
    return (machine == IMAGE_FILE_MACHINE_I386 && magic == 0x10b) ||
           (machine == IMAGE_FILE_MACHINE_AMD64 && magic == 0x20b &&
            system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
}

/* 先探测内存中的 PE 头，只有符合条件的附件才写入磁盘。 */
static int download(const wchar_t *url, wchar_t output[MAX_PATH]) {
    const wchar_t prefix[] = L"https://github.com/" OWNER L"/" REPO L"/releases/download/";
    HINTERNET connection = NULL, request = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    unsigned char buffer[4096];
    DWORD used = 0, count, written, length;
    wchar_t directory[MAX_PATH], temporary[MAX_PATH] = {0};
    int result = -1;
    output[0] = 0;
    if (wcsncmp(url, prefix, wcslen(prefix))) goto done;
    request = get(L"github.com", url + wcslen(L"https://github.com"), &connection);
    if (!request) goto done;
    while (used < sizeof(buffer)) {
        if (!WinHttpReadData(request, buffer + used, sizeof(buffer) - used, &count)) goto done;
        if (!count) break;
        used += count;
    }
    if (!executable(buffer, used)) { result = 0; goto done; }
    length = GetTempPathW(MAX_PATH, directory);
    if (!length || length >= MAX_PATH || !GetTempFileNameW(directory, L"gbr", 0, temporary)) goto done;
    if (wcslen(temporary) + 4 >= MAX_PATH) goto done;
    swprintf(output, MAX_PATH, L"%ls.exe", temporary);
    if (!MoveFileW(temporary, output)) goto done;
    file = CreateFileW(output, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) goto done;
    if (!WriteFile(file, buffer, used, &written, NULL) || written != used) goto done;
    for (;;) {
        if (!WinHttpReadData(request, buffer, sizeof(buffer), &count)) goto done;
        if (!count) break;
        if (!WriteFile(file, buffer, count, &written, NULL) || written != count) goto done;
    }
    result = 1;
done:
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (temporary[0]) DeleteFileW(temporary);
    if (result != 1 && output[0]) DeleteFileW(output);
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    return result;
}

static int run(const wchar_t *path) {
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    wchar_t command[MAX_PATH + 3];
    DWORD code;
    int success;
    startup.cb = sizeof(startup);
    swprintf(command, MAX_PATH + 3, L"\"%ls\"", path);
    if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        fwprintf(stderr, L"启动失败，系统错误码：%lu。\n", GetLastError());
        return 0;
    }
    success = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 &&
              GetExitCodeProcess(process.hProcess, &code);
    if (success) fwprintf(stdout, L"程序执行完成，退出码：%lu。\n", code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return success && code == 0;
}

int wmain(void) {
    char *release;
    const char *cursor;
    wchar_t url[2048], path[MAX_PATH];
    int found = 0, executed = 0, failed = 0, parsed;
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
    fwprintf(stdout, L"正在查询最新正式发布版本……\n");
    session = WinHttpOpen(L"github-binary-runner/3.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
    if (!session) return 1;
    WinHttpSetTimeouts(session, 30000, 30000, 30000, 30000);
    release = latest();
    if (!release) {
        fwprintf(stderr, L"无法获取最新版本，请检查网络或仓库是否已发布正式版本。\n");
        WinHttpCloseHandle(session);
        return 1;
    }
    cursor = release;
    while ((parsed = next_url(&cursor, url)) > 0) {
        int result;
        ++found;
        fwprintf(stdout, L"正在检查附件：%ls\n", url);
        result = download(url, path);
        if (result == 0) { fwprintf(stdout, L"跳过：不是本机支持的 Windows 可执行文件。\n"); continue; }
        if (result < 0) { fwprintf(stderr, L"附件下载失败。\n"); ++failed; continue; }
        fwprintf(stdout, L"正在执行二进制文件……\n");
        if (run(path)) ++executed; else ++failed;
        if (!DeleteFileW(path)) fwprintf(stderr, L"临时文件清理失败：%ls\n", path);
    }
    if (parsed < 0) { fwprintf(stderr, L"发布版本数据解析失败。\n"); ++failed; }
    free(release);
    WinHttpCloseHandle(session);
    fwprintf(stdout, L"处理完成：发现 %d 个附件，成功执行 %d 个，失败 %d 个。\n", found, executed, failed);
    return executed && !failed ? 0 : 1;
}
