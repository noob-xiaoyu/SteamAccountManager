# Steam Account Manager

![GitHub Pages](https://img.shields.io/badge/GitHub%20Pages-Deployed-brightgreen)
![Vue 3](https://img.shields.io/badge/Vue-3.5+-41b883)
![WebView2](https://img.shields.io/badge/WebView2-1.0+-0078d4)
![License](https://img.shields.io/badge/License-Proprietary-red)

一个现代化的 Steam 账号管理桌面应用，采用 C++ (WebView2) 作为客户端核心，配合 Vue 3 + Vite 构建现代化前端交互界面。

## 🌐 在线预览

前端界面已通过 GitHub Pages 部署，可在线访问： [在线预览](https://noob-xiaoyu.github.io/SteamAccountManager/)

> **注意**：在线预览仅展示前端界面功能，完整的 Steam 登录功能需要桌面客户端支持。

## ✨ 功能特性

### 🎯 账号管理
- **分类管理**：支持主账号、完美账号、5E账号、普通账号四种分类
- **批量操作**：支持批量添加、批量删除、批量编辑
- **状态管理**：账号状态标识（正常、冷却中、封禁、未知）
- **自动恢复**：自动检测并恢复到期冷却账号
- **密码管理**：密码字段加密显示/隐藏，支持复制功能

### 🔐 登录功能
- **一键登录**：快速切换到指定 Steam 账号
- **自动登录**：自动设置 Windows 注册表实现 Steam 自动登录
- **安全处理**：自动结束 Steam 进程并重新启动
- **兼容性**：支持 Steam 启动参数优化

### 🎨 界面体验
- **深色/浅色主题**：支持系统级主题切换
- **自定义窗口**：无边框窗口设计，支持拖拽、缩放
- **响应式布局**：侧边栏可折叠，适配不同屏幕尺寸
- **键盘快捷键**：支持 Ctrl+A、上下键导航、Delete 等快捷键
- **右键菜单**：上下文菜单提供快速操作

### 📊 数据管理
- **本地存储**：账号数据以 JSON 格式存储在本地
- **配置持久化**：主题、侧边栏状态等配置自动保存
- **数据隔离**：不同分类账号数据分开存储
- **备份兼容**：支持旧版本数据格式自动升级

## 🏗️ 技术架构

### 客户端核心 (C++ / WebView2)
```cpp
// 主要技术栈
- Microsoft WebView2 (Chromium 内核嵌入)
- Windows API (注册表操作、进程管理)
- nlohmann/json (JSON 数据处理)
- COM 编程模型
```

**核心功能模块**：
- **窗口管理**：无边框窗口、DPI 适配、阴影效果
- **WebView2 集成**：资源内嵌、消息通信、虚拟主机映射
- **Steam 集成**：注册表操作、进程控制、自动登录
- **数据持久化**：JSON 文件读写、配置管理
- **消息通信**：与前端 Vue 应用双向通信

### 前端界面 (Vue 3 / Vite)
```javascript
// 主要技术栈
- Vue 3 Composition API
- Vite 构建工具
- CSS3 自定义属性 (CSS Variables)
- WebView2 JavaScript API
```

**核心功能模块**：
- **组件架构**：单文件组件 (SFC) 设计
- **状态管理**：响应式数据绑定、计算属性
- **UI/UX**：自定义滚动条、动画过渡、模态对话框
- **通信层**：WebMessage 与 C++ 后端通信
- **构建优化**：单文件打包、资源内联

### 数据存储结构
```json
// 账号数据示例
{
  "main": [
    {
      "id": "unique_id",
      "username": "steam_user",
      "password": "encrypted_password",
      "alias": "我的主账号",
      "status": "正常",
      "steamId": "76561198000000001",
      "category": "main",
      "cooldownUntil": "2026-12-31T23:59"
    }
  ],
  "perfect": [],
  "5e": [],
  "normal": []
}
```

## 🚀 快速开始

### 环境要求
- **操作系统**: Windows 10/11 (64位)
- **运行时**: Microsoft Edge WebView2 Runtime
- **开发环境** (可选):
  - Visual Studio 2022 (C++ 桌面开发)
  - Node.js 20+ (前端开发)

### 客户端构建
1. **克隆项目**:
   ```bash
   git clone https://github.com/noob-xiaoyu/SteamAccountManager.git
   cd SteamAccountManager
   ```

2. **前端构建**:
   ```bash
   cd frontend
   npm install
   npm run build
   ```
   > 构建后的单文件 HTML 将自动嵌入到 C++ 资源中

3. **客户端编译**:
   - 使用 Visual Studio 2022 打开 `SteamAccountManager.sln`
   - 选择 Release x64 配置
   - 编译生成 `SteamAccountManager.exe`

### 运行使用
1. **首次运行**：确保系统已安装 WebView2 Runtime
2. **添加账号**：使用批量添加功能导入账号数据
3. **登录操作**：选择账号点击登录按钮
4. **数据管理**：所有更改自动保存到本地 JSON 文件

## 📖 使用指南

### 账号管理操作
1. **批量添加账号**:
   - 点击 "➕批量添加➕" 按钮
   - 输入格式: `用户名----密码` 或 `用户名`
   - 支持分隔符: `----`, `---`, `--`, `-`, `，`, `,`
   - 每行一个账号，自动识别分类

2. **账号编辑**:
   - 单击选择单个账号，Ctrl+单击多选，Shift+单击范围选择
   - 右键菜单提供快速操作
   - 双击或按 Enter 键编辑选中账号

3. **账号删除**:
   - 选择账号后点击 "🗑️删除选中"
   - 支持批量删除操作
   - 删除前有确认提示

### 登录功能
1. **单账号登录**:
   - 在账号行点击 "登录" 按钮
   - 或右键菜单选择 "🚀 登录"
   - 系统将自动结束当前 Steam 进程并登录新账号

2. **登录流程**:
   ```
   1. 结束 Steam 进程 (如果正在运行)
   2. 设置注册表 AutoLoginUser 和 RememberPassword
   3. 启动 Steam 并传递登录参数
   4. 显示登录状态提示
   ```

### 主题与设置
1. **主题切换**:
   - 点击标题栏设置按钮
   - 选择 "☀️ 浅色主题" 或 "🌙 深色主题"
   - 主题设置自动保存

2. **窗口操作**:
   - 标题栏拖拽：拖动窗口
   - 标题栏双击：最大化/还原
   - 边缘拖拽：调整窗口大小
   - 控制按钮：最小化、最大化、关闭

### 键盘快捷键
| 快捷键 | 功能描述 |
|--------|----------|
| `Ctrl + A` | 全选当前分类所有账号 |
| `↑ / ↓` | 上下导航选择账号 |
| `Enter` | 编辑选中账号 |
| `Delete` / `Del` | 删除选中账号 |
| `Escape` | 取消选择/关闭面板 |
| `Ctrl + D` | 删除选中账号 (替代) |

## 🔧 开发指南

### 项目结构
```
SteamAccountManager/
├── SteamAccountManager/          # C++ 客户端项目
│   ├── SteamAccountManager.cpp   # 主程序入口
│   ├── SteamAccountManager.h     # 头文件
│   ├── SteamAccountManager.rc    # 资源文件
│   ├── SteamAccountManager.vcxproj # VS 项目文件
│   └── include/nlohmann/         # JSON 库
├── frontend/                     # Vue 前端项目
│   ├── src/
│   │   ├── App.vue              # 主组件
│   │   ├── main.js              # 入口文件
│   │   ├── style.css            # 全局样式
│   │   └── assets/              # 静态资源
│   ├── index.html               # HTML 模板
│   ├── vite.config.js           # Vite 配置
│   └── package.json             # 依赖配置
├── packages/                     # NuGet 包依赖
│   ├── Microsoft.Web.WebView2/  # WebView2 包
│   └── Microsoft.Windows.ImplementationLibrary/ # WIL 包
└── .github/workflows/           # GitHub Actions
    └── deploy.yml               # 自动部署配置
```

### 前后端通信协议
**前端 → 后端** (JavaScript):
```javascript
// 获取账号数据
window.chrome.webview.postMessage({
  type: 'getAccounts'
});

// 保存账号数据
window.chrome.webview.postMessage({
  type: 'saveAccounts',
  accounts: accountList
});

// 登录请求
window.chrome.webview.postMessage({
  type: 'login',
  username: 'steam_user',
  password: 'user_password'
});

// 窗口控制
window.chrome.webview.postMessage({
  type: 'windowControl',
  action: 'minimize' // minimize/maximize/close/startDrag
});
```

**后端 → 前端** (C++):
```cpp
// 发送账号数据
json response = {
  {"type", "setAccounts"},
  {"accounts", accountsData}
};
webview->PostWebMessageAsJson(StringToWString(response.dump()));

// 发送配置数据
json config = {
  {"type", "setConfig"},
  {"config", configData}
};
webview->PostWebMessageAsJson(StringToWString(config.dump()));
```

### 数据存储位置
- **账号数据**: `%EXE_DIR%\accounts.json`
- **配置文件**: `%EXE_DIR%\config.json`
- **WebView2 缓存**: `%TEMP%\SteamAccountManager_WebView2\`

## 🛠️ 构建与部署

### 前端构建配置
```javascript
// vite.config.js
export default defineConfig({
  plugins: [vue(), viteSingleFile()], // 单文件打包
  base: process.env.VITE_BASE_PATH || '/',
  build: {
    assetsInlineLimit: 100000000, // 图片转为 Base64
  }
});
```

### 客户端资源嵌入
C++ 项目将构建后的 HTML 文件作为资源嵌入：
```rc
// SteamAccountManager.rc
IDR_HTML1 HTML "frontend\\dist\\index.html"
```

### 自动部署流程
GitHub Actions 自动构建前端并部署到 GitHub Pages:
```yaml
# .github/workflows/deploy.yml
name: Deploy to GitHub Pages
on: [push, workflow_dispatch]
jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
      - run: npm ci && npm run build
        working-directory: frontend
      - uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: frontend/dist
          publish_branch: gh-pages
```

## 📋 版本历史

### v1.0.0 (当前版本)
- ✅ 基础账号管理功能
- ✅ Steam 自动登录集成
- ✅ 深色/浅色主题支持
- ✅ 响应式界面设计
- ✅ 本地数据持久化
- ✅ 键盘快捷键支持
- ✅ 右键上下文菜单
- ✅ 批量操作功能

## 🤝 贡献指南

### 开发贡献
1. Fork 本仓库
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 问题反馈
- **Bug 报告**: 在 Issues 中描述问题现象、复现步骤
- **功能建议**: 详细描述需求场景和使用价值
- **代码改进**: 提供优化方案和测试结果

## ⚠️ 注意事项

### 安全提醒
1. **密码安全**: 账号密码以明文存储在本地 JSON 文件中，请注意数据安全
2. **账户风险**: 频繁切换 Steam 账号可能触发安全验证
3. **权限要求**: 软件需要管理员权限修改注册表和结束进程

### 兼容性说明
1. **Steam 版本**: 支持最新版 Steam 客户端
2. **系统要求**: 需要 Windows 10/11 64位系统
3. **运行时依赖**: 必须安装 WebView2 Runtime

### 法律声明
本工具仅用于合法的账号管理需求，请遵守 Steam 用户协议和相关法律法规。开发者不对滥用本工具造成的任何后果负责。

## 📄 许可证

**~~⚠️ 本项目为闭源（非开源）项目。~~   不！，现在开源了** 

- ~~未经原作者明确的书面许可，严禁任何人以任何形式复制、分发、修改、展示、反编译或将本项目的源代码及相关资产用于商业或非商业用途。~~
- ~~本项目的源代码、架构设计及相关资源均受著作权法及知识产权相关法律保护。~~
- **~~保留所有权利 (Copyright © All Rights Reserved)~~**。

## 👥 作者与致谢

- **开发者**: [noob-xiaoyu](https://github.com/noob-xiaoyu)
- **技术栈**: Vue 3, Vite, C++, WebView2, Windows API
- **特别感谢**: Microsoft WebView2 团队, Vue.js 社区

---

**Steam Account Manager** - 简洁高效的 Steam 账号管理解决方案

> 如有问题或建议，欢迎在 GitHub 仓库中提出 Issue 或讨论。