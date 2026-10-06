#ifndef GITHUB_TOKEN_H
#define GITHUB_TOKEN_H

#include <windows.h>
#include <stdio.h>
#include <wchar.h>

/* 仅保存在进程内存中；不设置任何环境变量。 */
static wchar_t github_token[4097];

static int token_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int parse_github_token(const unsigned char *data, DWORD size) {
    DWORD begin = 0, end = size, i;
    SecureZeroMemory(github_token, sizeof(github_token));
    if (size > 4096) return 0;
    if (size >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf) begin = 3;
    while (begin < end && token_space(data[begin])) ++begin;
    while (end > begin && token_space(data[end - 1])) --end;
    if (begin == end) return 0;
    for (i = begin; i < end; ++i) {
        unsigned char c = data[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    for (i = begin; i < end; ++i) github_token[i - begin] = (wchar_t)data[i];
    return 1;
}

/* 0：文件不存在；1：已读取；-1：读取或格式错误。 */
static int load_github_token_file(const wchar_t *path) {
    unsigned char data[4097];
    DWORD count = 0, total = 0, error;
    HANDLE file;
    int result = -1;
    SecureZeroMemory(github_token, sizeof(github_token));
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? 0 : -1;
    }
    while (total < sizeof(data)) {
        if (!ReadFile(file, data + total, (DWORD)sizeof(data) - total, &count, NULL)) goto done;
        if (!count) break;
        total += count;
    }
    result = parse_github_token(data, total) ? 1 : -1;
done:
    CloseHandle(file);
    SecureZeroMemory(data, sizeof(data));
    return result;
}

static void init_github_token(void) {
    DWORD drives = GetLogicalDrives();
    int i, seen = 0;
    SecureZeroMemory(github_token, sizeof(github_token));
    for (i = 0; i < 26; ++i) {
        wchar_t root[] = L"A:\\", label[MAX_PATH], path[MAX_PATH];
        UINT type;
        int result;
        if (!(drives & (1UL << i))) continue;
        root[0] = (wchar_t)(L'A' + i);
        type = GetDriveTypeW(root);
        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) continue;
        if (!GetVolumeInformationW(root, label, MAX_PATH, NULL, NULL, NULL, NULL, 0) ||
            _wcsicmp(label, L"Ventoy")) continue;
        seen = 1;
        swprintf(path, MAX_PATH, L"%lsapi_key_github.txt", root);
        result = load_github_token_file(path);
        if (result == 1) {
            fwprintf(stdout, L"已读取 Ventoy 中的 GitHub 令牌，仅用于本进程认证。\n");
            return;
        }
        if (result < 0) fwprintf(stderr, L"Ventoy 令牌文件无法读取或格式无效，跳过认证设置。\n");
    }
    fwprintf(stdout, seen ? L"未读取到 Ventoy 令牌文件，继续匿名访问。\n" :
                           L"未找到 Ventoy 卷，继续匿名访问。\n");
}

#endif
