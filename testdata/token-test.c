#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include "../src/github-token.h"

#define CHECK(condition) do { if (!(condition)) { fwprintf(stderr, L"令牌测试失败，源码行号：%d\n", __LINE__); return 1; } } while (0)

int wmain(void) {
    const unsigned char good[] = "\xef\xbb\xbf  ghp_TEST_ONLY_123\r\n";
    unsigned char large[4097];
    wchar_t directory[MAX_PATH], path[MAX_PATH], before[32767], after[32767];
    HANDLE file;
    DWORD written, previous, current;
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stderr), _O_U8TEXT);
    previous = GetEnvironmentVariableW(L"GITHUB_TOKEN", before, 32767);
    init_github_token();
    CHECK(parse_github_token(good, (DWORD)sizeof(good) - 1));
    CHECK(!wcscmp(github_token, L"ghp_TEST_ONLY_123"));
    CHECK(!parse_github_token((const unsigned char *)"   \r\n", 5));
    CHECK(!github_token[0]);
    CHECK(!parse_github_token((const unsigned char *)"one\r\ntwo", 8));
    CHECK(!parse_github_token((const unsigned char *)"one\0two", 7));
    memset(large, 'a', sizeof(large));
    CHECK(!parse_github_token(large, (DWORD)sizeof(large)));
    CHECK(GetTempPathW(MAX_PATH, directory));
    CHECK(GetTempFileNameW(directory, L"gtt", 0, path));
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(file != INVALID_HANDLE_VALUE);
    CHECK(WriteFile(file, good, (DWORD)sizeof(good) - 1, &written, NULL));
    CloseHandle(file);
    CHECK(load_github_token_file(path) == 1);
    CHECK(!wcscmp(github_token, L"ghp_TEST_ONLY_123"));
    current = GetEnvironmentVariableW(L"GITHUB_TOKEN", after, 32767);
    CHECK(previous == current && (!previous || !wcscmp(before, after)));
    CHECK(DeleteFileW(path));
    CHECK(load_github_token_file(path) == 0);
    CHECK(!github_token[0]);
    fwprintf(stdout, L"令牌解析、缺失文件和进程环境隔离测试通过。\n");
    return 0;
}
