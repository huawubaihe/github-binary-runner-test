# Ventoy Windows Release 加载器

目标 Windows 10 x64。扫描卷标 Ventoy，读取根目录 api_key_github.txt，启动 mihomo/mihomo.exe 和 mihomo/config.yaml，通过本地 HTTP 代理查询 huawubaihe/github-binary-runner-test 最新正式 Release。所有提示为中文，无 SHA-256 校验，不执行 shell，不使用镜像。

## U 盘布局

```text
Ventoy:\
  loader.exe
  api_key_github.txt
  mihomo\
    mihomo.exe
    config.yaml
```

节点配置只保留美国S01一项，通过 MATCH 规则固定该节点。不依赖在线订阅更新、Geo 数据或规则集。仓库 config.example.yaml 只含占位符；真实订阅、节点凭据和令牌不上传。

加载器检查 17890 端口空闲，启动 Mihomo 官方 Windows amd64-compatible 核心，并通过监听 PID 确認端口属于它启动的核心。WinHTTP 会话显式使用 127.0.0.1:17890，不修改系统代理或环境变量，不启用 TUN，附件程序不自动使用代理。节点或配置失败时停止，不回退直连。Job Object 在加载器退出或被终止时结束核心；端口就绪不等于节点连通，真实 GitHub 查询用于验证连通性。固定节点由预置的单节点配置保证，用户自行改动配置会改变行为。

令牌读取支持 ASCII/UTF-8、BOM、首尾空白，最大 4096 字节。文件缺失或格式无效时匿名访问。令牌仅存在加载器内部 github_token 变量，用于 api.github.com 认证，查询后清零，不设置 GITHUB_TOKEN 或传给子进程。当前附件下载仅支持公开 Release。

附件先在内存读取最多 4096 字节识别 PE，仅将兼容 x86/x64 GUI 或控制台 EXE 写入临时目录并执行。DLL、文本、脚本、压缩包、伪装 EXE 和不支持架构跳过。等待执行结束后清理文件。成品静态链接 C 运行库，使用 Windows 自带 WinHTTP 和 IP Helper API；附件程序自身依赖仍由发布者处理。

## 构建和验证

MinGW-w64：make。

MSVC：

```text
cl /nologo /O2 /MT /utf-8 /D_CRT_SECURE_NO_WARNINGS src/main.c /Fe:runner.exe /link winhttp.lib iphlpapi.lib
```

CI 创建临时 Ventoy VHD，安装 Mihomo，使用无用户凭据的 DIRECT 出站测试配置，验证代理隧道、真实 Release 下载执行两次、令牌解析、核心退出清理、端口冲突与配置缺失。测试配置用于隔离验证，不证明用户美国节点可用。runner.exe 在 Actions Artifact 中，不放进加载器消费的 Release，避免递归执行。Windows-latest CI 不等同于干净 Windows 10 实测。
