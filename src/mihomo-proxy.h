#ifndef MIHOMO_PROXY_H
#define MIHOMO_PROXY_H

#include <windows.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define MIHOMO_PORT 17890
#define MIHOMO_ADDRESS L"127.0.0.1:17890"
static HANDLE proxy_job, proxy_process;

static void stop_proxy(void) {
    if (proxy_job) {
        /* Job 关闭或加载器被终止时，系统结束我们启动的核心及其子进程。 */
        CloseHandle(proxy_job);
        proxy_job = NULL;
    }
    if (proxy_process) {
        WaitForSingleObject(proxy_process, 5000);
        CloseHandle(proxy_process);
        proxy_process = NULL;
    }
}

/* 返回端口监听进程 ID；0 表示空闲，查询失败则返回 -1。 */
static DWORD proxy_port_owner(void) {
    DWORD size = 0, error, i, owner = 0;
    MIB_TCPTABLE_OWNER_PID *table;
    error = GetExtendedTcpTable(NULL, &size, FALSE, 2, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (error != ERROR_INSUFFICIENT_BUFFER) return (DWORD)-1;
    table = (MIB_TCPTABLE_OWNER_PID *)malloc(size);
    if (!table) return (DWORD)-1;
    error = GetExtendedTcpTable(table, &size, FALSE, 2, TCP_TABLE_OWNER_PID_LISTENER, 0);
    if (error != NO_ERROR) { free(table); return (DWORD)-1; }
    for (i = 0; i < table->dwNumEntries; ++i) {
        DWORD port = table->table[i].dwLocalPort & 0xffff;
        if (port == ((MIHOMO_PORT >> 8) | ((MIHOMO_PORT & 255) << 8)) &&
            (table->table[i].dwLocalAddr == 0x0100007f || table->table[i].dwLocalAddr == 0)) {
            owner = table->table[i].dwOwningPid;
            break;
        }
    }
    free(table);
    return owner;
}

static int find_mihomo_directory(wchar_t directory[MAX_PATH]) {
    DWORD drives = GetLogicalDrives();
    int i;
    for (i = 0; i < 26; ++i) {
        wchar_t root[] = L"A:\\", label[MAX_PATH], executable_path[MAX_PATH], config[MAX_PATH];
        DWORD exe_attributes, config_attributes;
        UINT type;
        if (!(drives & (1UL << i))) continue;
        root[0] = (wchar_t)(L'A' + i);
        type = GetDriveTypeW(root);
        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) continue;
        if (!GetVolumeInformationW(root, label, MAX_PATH, NULL, NULL, NULL, NULL, 0) ||
            _wcsicmp(label, L"Ventoy")) continue;
        swprintf(directory, MAX_PATH, L"%lsmihomo", root);
        swprintf(executable_path, MAX_PATH, L"%ls\\mihomo.exe", directory);
        swprintf(config, MAX_PATH, L"%ls\\config.yaml", directory);
        exe_attributes = GetFileAttributesW(executable_path);
        config_attributes = GetFileAttributesW(config);
        if (exe_attributes != INVALID_FILE_ATTRIBUTES && !(exe_attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            config_attributes != INVALID_FILE_ATTRIBUTES && !(config_attributes & FILE_ATTRIBUTE_DIRECTORY)) return 1;
    }
    return 0;
}

static int start_proxy(void) {
    wchar_t directory[MAX_PATH], exe[MAX_PATH], config[MAX_PATH], command[MAX_PATH * 3 + 64];
    STARTUPINFOW startup = {0};
    PROCESS_INFORMATION process = {0};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
    SECURITY_ATTRIBUTES security = {sizeof(security), NULL, TRUE};
    HANDLE null_file;
    DWORD owner = proxy_port_owner(), start;
    if (owner) {
        fwprintf(stderr, L"本地代理端口已占用或无法检查，停止加载，请检查端口 17890。\n");
        return 0;
    }
    if (!find_mihomo_directory(directory)) {
        fwprintf(stderr, L"未找到 Ventoy\\mihomo 中的核心或配置文件，停止加载。\n");
        return 0;
    }
    swprintf(exe, MAX_PATH, L"%ls\\mihomo.exe", directory);
    swprintf(config, MAX_PATH, L"%ls\\config.yaml", directory);
    swprintf(command, MAX_PATH * 3 + 64, L"\"%ls\" -d \"%ls\" -f \"%ls\"", exe, directory, config);
    proxy_job = CreateJobObjectW(NULL, NULL);
    if (!proxy_job) return 0;
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(proxy_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) goto failed;
    null_file = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            &security, OPEN_EXISTING, 0, NULL);
    if (null_file == INVALID_HANDLE_VALUE) goto failed;
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = startup.hStdOutput = startup.hStdError = null_file;
    if (!CreateProcessW(exe, command, NULL, NULL, TRUE, CREATE_SUSPENDED | CREATE_NO_WINDOW,
                        NULL, directory, &startup, &process)) {
        CloseHandle(null_file);
        goto failed;
    }
    CloseHandle(null_file);
    proxy_process = process.hProcess;
    if (!AssignProcessToJobObject(proxy_job, process.hProcess)) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        goto failed;
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) { CloseHandle(process.hThread); goto failed; }
    CloseHandle(process.hThread);
    start = GetTickCount();
    fwprintf(stdout, L"正在启动本地代理，固定节点为美国S01……\n");
    while (GetTickCount() - start < 20000) {
        owner = proxy_port_owner();
        if (owner == process.dwProcessId) {
            fwprintf(stdout, L"本地代理已就绪，仅供加载器使用。\n");
            return 1;
        }
        if (owner || WaitForSingleObject(proxy_process, 0) != WAIT_TIMEOUT) break;
        Sleep(100);
    }
failed:
    fwprintf(stderr, L"Mihomo 启动失败或代理端口未就绪，请检查核心和节点配置。\n");
    stop_proxy();
    return 0;
}

#endif
