# Windows 最新 Release 执行器

运行执行器即可查询 huawubaihe/github-binary-runner-test 的最新正式 Release，逐个执行兼容本机的 Windows PE 程序。每次启动重新查询。所有提示为中文，不进行 SHA-256 校验。当前版本面向公开仓库。

## Ventoy 令牌文件

启动时扫描卷标为 Ventoy 的本地盘，包括可移动盘和被 Windows 识别为固定盘的 USB 盘。读取根目录 api_key_github.txt，内容为单个 GitHub token，使用 ASCII 或 UTF-8，可带 UTF-8 BOM、首尾空格和换行，最大 4096 字节。文件不存在、为空、无法读取或格式无效时跳过设置，继续匿名访问。多个 Ventoy 卷按盘符顺序查找首个有效文件。

令牌仅保存在加载器进程内的 github_token 变量，用于 api.github.com 的 Authorization 请求头，不设置 GITHUB_TOKEN 环境变量、不修改系统或用户环境、不传给子进程、不输出令牌内容。API 查询结束后清零内存。附件下载仍使用公开 URL；提供令牌并不使当前版本完整支持私有 Release 下载。

## 附件筛选

先在内存探测附件前 4096 字节，检查 MZ、PE 签名、架构、可执行标志和 GUI/控制台子系统。DLL、文本、脚本、压缩包和不兼容架构跳过，不写入磁盘；文本伪装 EXE 同样跳过。符合条件的程序下载到临时 EXE，用 CreateProcessW 启动，等待退出并删除。非 PE 附件需要读取少量内容以判断格式。

成品要求 Windows 10 x64；支持执行 x86/x64 程序。执行器使用系统 WinHTTP，并静态链接 C 运行库，不需要 Git、Python、额外 .NET 或 VC 运行库安装。附件程序自身的外部依赖仍由发布者负责。

## 自动构建与验证

main 推送或 workflow_dispatch 在 Windows runner 上通过 MSVC /MT 编译，验证令牌解析、缺失文件和环境隔离，然后发布包含 harmless.exe、sample.dll、notes.txt、fake.exe 的正式 Release。连续启动执行器两次，检查只执行 harmless.exe、测试标记和临时文件清理。runner.exe 保存为 Actions 中的 Windows执行器 Artifact，不放入自己消费的 Release。

## 构建

MinGW-w64：make。

MSVC 开发者命令行：

```text
cl /nologo /O2 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS src/main.c /Fe:runner.exe /link winhttp.lib
```

仅在信任发布者时运行。最新版本采用 GitHub releases/latest 定义，不包含草稿或预发布。CI 使用 windows-latest，通过不等于已在干净 Windows 10 或真实 Ventoy U 盘上实测。
