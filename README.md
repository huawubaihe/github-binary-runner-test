# Windows 最新 Release 执行器

运行执行器即可查询 huawubaihe/github-binary-runner-test 的最新正式 Release，逐个执行兼容本机的 Windows PE 程序。每次启动重新查询。所有提示为中文，不进行 SHA-256 校验。当前支持公开仓库，无需 token。

先在内存探测附件前 4096 字节，检查 MZ、PE 签名、架构、可执行标志和 GUI/控制台子系统。DLL、文本、脚本、压缩包和不兼容架构跳过，不写入磁盘；文本伪装 EXE 同样跳过。符合条件的程序下载到临时 EXE，用 CreateProcessW 启动，等待退出并删除。非 PE 附件需要读取少量内容以判断格式。

成品要求 Windows 10 x64；支持执行 x86/x64 程序。执行器使用系统 WinHTTP，并静态链接 C 运行库，不需要 Git、Python、额外 .NET 或 VC 运行库安装。附件程序自身的外部依赖仍由发布者负责。

## 自动构建与验证

main 推送或 workflow_dispatch 在 Windows runner 上通过 MSVC /MT 编译，发布包含 harmless.exe、sample.dll、notes.txt、fake.exe 的正式 Release。随后连续启动执行器两次，检查只执行 harmless.exe、测试标记和临时文件清理。runner.exe 保存为 Actions 中的 Windows执行器 Artifact，不放入它自己消费的 Release，避免递归执行。

## 构建

MinGW-w64：make。

MSVC 开发者命令行：

```text
cl /nologo /O2 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS src/main.c /Fe:runner.exe /link winhttp.lib
```

只在信任 Release 发布者时运行。最新版本采用 GitHub releases/latest 定义，不包含草稿或预发布。CI 使用 windows-latest，其通过不等于已在干净 Windows 10 设备上实测。
