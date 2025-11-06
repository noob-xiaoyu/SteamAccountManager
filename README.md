# SteamAccountManager
## ✨ 主要功能

- **账号集中管理**: 在一个清晰的列表中管理您所有的 Steam 账号信息，包括用户名、密码、昵称、邮箱等。
- **智能排序与高亮**:
  - 自动根据账号状态（正常、冷却中、VAC封禁）进行排序，重要状态一目了然。
  - 对处于冷却或封禁状态的账号整行进行颜色高亮，提供强烈的视觉警示。
- **一键安全登录**:
  - 自动检测本地 Steam 客户端路径。
  - 在登录前检测 Steam 是否已在运行，并提示用户关闭现有进程，确保切换账号成功。
  - 通过调用官方 Steam 客户端并传递登录参数来实现登录，**全程不直接处理 Steam Guard 验证码**，安全可靠。
- **批量操作**:
  - 支持通过逗号或特定分隔符（`----`）批量添加账号，兼容多种格式。
- **API 集成**:
  - 通过 Steam Web API 批量更新所有账号的最新昵称和封禁状态。
  - 智能填充功能：当您粘贴包含 API Key 的文本时，程序会自动提取出 32 位的纯净 Key。
- **数据持久化**:
  - 所有账号信息和应用设置（如 API Key）都会自动保存在程序根目录的 `.json` 文件中。
  - **自动保存**: 关闭程序时，所有更改都会自动静默保存，无需手动操作。
- **便捷的交互**:
  - 支持通过顶部按钮或右键上下文菜单对账号进行更改、登录、删除等操作。
  - 点击昵称可直接在浏览器中打开对应的 Steam 社区个人资料页面。
## 🚀 如何开始

### 使用方法

1.  从 **[Releases](https://github.com/noob-xiaoyu/SteamAccount/releases)** 页面下载最新的 `.exe`可执行文件。
2.  运行 `SteamAccountManager.exe`。

### 首次运行

- 程序首次启动时，会自动在程序目录下创建 `accounts.json` (用于存储账号) 和 `config.json` (用于存储设置)。
- 程序会自动尝试从 Windows 注册表检测您的 `Steam.exe` 路径。

### 获取并设置 Steam API Key

为了使用“更新昵称”和“更新封禁状态(无法获取时间)”功能，您需要一个 Steam Web API 密钥。
1.  **[获取Steam API](./Get-Steam-API/Get_Steam-API.md)

## 📦 项目结构

项目遵循标准的 MVVM 设计模式，结构清晰：
- `/Models`: 定义数据模型，如 `SteamAccount.cs`。
- `/ViewModels`: 存放视图模型，`MainViewModel.cs` 是核心业务逻辑所在。
- `/Views` (或根目录): 包含所有的 WPF 窗口和 XAML 界面定义。
- `/Services`: 处理与外部服务的交互，如 `SteamApiService.cs`。
- `/Commands`: 存放 `RelayCommand` 等命令实现。
- `/Utils`: 包含辅助工具类，如 `RegistryHelper.cs`。
- `/Converters`: 包含 WPF 值转换器。

## 🤝 贡献

欢迎提交 Pull Request 或创建 Issue 来为这个项目做出贡献。

## 🤝 待完善
- 颜色高亮
- 右键处理
- 冷却过期-->正常
- 一坨

## 📜 许可证

本项目采用 [MIT License]()。
