// SteamAccountManager.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "SteamAccountManager.h"
#include <wrl.h>
#include <wil/com.h>
#include <WebView2.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <shellapi.h>
#include <winreg.h>
#include <filesystem>
#include <tlhelp32.h>
#include <thread>
#include <nlohmann/json.hpp>
#include <dwmapi.h>
#include <windowsx.h>
#include <commctrl.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

using namespace Microsoft::WRL;
using json = nlohmann::json;

#define MAX_LOADSTRING 100

#define w 1570
#define h 950

// 辅助函数：wstring 转 UTF-8 string
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// 辅助函数：UTF-8 string 转 wstring
std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// 子类化回调：处理 WebView2 控件的消息穿透
LRESULT CALLBACK Webview2SubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_NCHITTEST) {
        HWND hMainWnd = (HWND)dwRefData;
        
        // 关键：主动询问主窗口——“如果这消息是你收到的，你会返回什么？”
        LRESULT hit = SendMessage(hMainWnd, WM_NCHITTEST, wParam, lParam);
        
        // 如果主窗口认为这里是边框（缩放区）或标题栏（拖拽区），
        // 那么子窗口必须返回 HTTRANSPARENT，消息才能到达主窗口。
        if (hit != HTCLIENT) {
            return HTTRANSPARENT;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// 辅助函数：递归子类化所有子窗口
BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    SetWindowSubclass(hwnd, Webview2SubclassProc, 1, (DWORD_PTR)lParam);
    return TRUE;
}

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
bool g_isDarkTheme = true;                      // 主题状态
bool g_isHoveringWindowControls = false;        // 窗口控制按钮悬浮状态
bool g_isTrackingMouse = false;                 // 鼠标追踪状态

// WebView2 变量
wil::com_ptr<ICoreWebView2Controller> webviewController;
wil::com_ptr<ICoreWebView2> webview;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

// Steam 相关函数
std::wstring PathToUrl(std::wstring path) {
    for (auto& c : path) {
        if (c == L'\\') c = L'/';
    }
    return L"file:///" + path;
}
// 设置自动登录用户名
void SetAutoLoginUser(const std::wstring& username) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        // 设置自动登录用户名
        RegSetValueExW(hKey, L"AutoLoginUser", 0, REG_SZ, (BYTE*)username.c_str(), (DWORD)(username.length() + 1) * sizeof(wchar_t));
        
        // 关键：必须设置 RememberPassword 为 1，否则 Steam 会忽略 AutoLoginUser
        DWORD remember = 1;
        RegSetValueExW(hKey, L"RememberPassword", 0, REG_DWORD, (BYTE*)&remember, sizeof(DWORD));
        
        RegCloseKey(hKey);
    }
}
// 获取 Steam 安装路径
std::wstring GetSteamPath() {
    HKEY hKey;
    WCHAR path[MAX_PATH];
    DWORD size = sizeof(path);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"SteamExe", 0, NULL, (BYTE*)path, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return std::wstring(path);
        }
        RegCloseKey(hKey);
    }
    return L"";
}
// 检查 Steam 是否正在运行
bool IsSteamRunning() {
    bool running = false;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(hSnap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) {
                    running = true;
                    break;
                }
            } while (Process32NextW(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return running;
}
// 启动 Steam
void StartSteam(const std::wstring& username, const std::wstring& password) {
    std::wstring steamPath = GetSteamPath();

    for (auto& c : steamPath) {
        if (c == L'/') c = L'\\';
    }

    if (!steamPath.empty()) {
        std::filesystem::path p(steamPath);
        std::wstring steamDir = p.parent_path().wstring();

        std::wstring args = L"-no-browser -skipinitialbootstrap -nobootstrapupdate -noreactlogin -login " + username + L"";

        if (!password.empty()) {
            args += L" " + password + L"";
        }

        std::wstring cmdLine = L"cmd /c start \"\" \"" + steamPath + L"\" " + args;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = { 0 };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        CreateProcessW(NULL, cmdLine.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, steamDir.c_str(), &si, &pi);
        if (pi.hProcess) CloseHandle(pi.hProcess);
        if (pi.hThread) CloseHandle(pi.hThread);
    }
}
// 关闭 Steam
void KillSteam() {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"steam.exe") == 0) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

// 账号存储 (简单文件存储)
std::wstring GetAccountsFilePath() {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path p(path);
    return (p.parent_path() / L"accounts.json").wstring();
}

// 配置存储路径
std::wstring GetConfigFilePath() {
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path p(path);
    return (p.parent_path() / L"config.json").wstring();
}

// 读取配置
json ReadConfigJson() {
    try {
        std::filesystem::path path = GetConfigFilePath();
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) == 0) {
            return json::object();
        }

        std::ifstream file(path.wstring(), std::ios::binary);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                file.close();
                return j;
            }
            catch (const std::exception& e) {
                file.close();
                return json::object();
            }
        }
    }
    catch (...) {
        return json::object();
    }
    return json::object();
}

// 保存配置
void SaveConfigJson(const json& j) {
    try {
        std::filesystem::path path = GetConfigFilePath();
        std::ofstream file(path.wstring(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (file.is_open()) {
            std::string content = j.dump(4);
            file.write(content.c_str(), content.length());
            file.close();
        }
    }
    catch (const std::exception& e) {
        std::wstring err = L"Config Save Error: " + StringToWString(e.what());
        MessageBoxW(NULL, err.c_str(), L"Error", MB_OK);
    }
}

// 读取账号
json ReadAccountsJson() {
    try {
        std::filesystem::path path = GetAccountsFilePath();
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) == 0) {
            return json::array();
        }

        std::ifstream file(path.wstring(), std::ios::binary);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                file.close();
                
                // 将分离存储的数据合并为前端所需的一维数组
                json result = json::array();
                if (j.is_object()) {
                    if (j.contains("main") && j["main"].is_array()) {
                        for (auto item : j["main"]) {
                            item["category"] = "main";
                            result.push_back(item);
                        }
                    }
                    if (j.contains("perfect") && j["perfect"].is_array()) {
                        for (auto item : j["perfect"]) {
                            item["category"] = "perfect";
                            result.push_back(item);
                        }
                    }
                    if (j.contains("5e") && j["5e"].is_array()) {
                        for (auto item : j["5e"]) {
                            item["category"] = "5e";
                            result.push_back(item);
                        }
                    }
                    if (j.contains("normal") && j["normal"].is_array()) {
                        for (auto item : j["normal"]) {
                            item["category"] = "normal";
                            result.push_back(item);
                        }
                    }
                    return result;
                }

                // 兼容旧版本格式
                if (j.is_array()) {
                    for (auto& item : j) {
                        if (item.value("isMain", false)) {
                            item["category"] = "main";
                        } else {
                            item["category"] = "normal";
                        }
                        // 为没有 id 的旧账号生成唯一 id
                        if (!item.contains("id") || item["id"].is_null()) {
                            item["id"] = "legacy_" + std::to_string(std::hash<std::string>{}(item.value("username", "")));
                        }
                    }
                    return j;
                }
                return json::array();
            }
            catch (const std::exception& e) {
                file.close();
                // 如果解析失败，可能是编码问题，备份旧文件并返回空
                std::filesystem::rename(path, path.wstring() + L".bak");
                // 转义内容：“检测到账号文件格式不兼容，已自动重置并备份旧文件。”，“数据恢复”
                MessageBoxW(NULL, L"\u68c0\u6d4b\u5230\u8d26\u53f7\u6587\u4ef6\u683c\u5f0f\u4e0d\u517c\u5bb9\uff0c\u5df2\u81ea\u52a8\u91cd\u7f6e\u5e76\u5907\u4efd\u65e7\u6587\u4ef6\u3002", L"\u6570\u636e\u6062\u590d", MB_OK | MB_ICONWARNING);
                return json::array();
            }
        }
    }
    catch (...) {
        return json::array();
    }
    return json::array();
}
// 保存账号
void SaveAccountsJson(const json& j) {
    try {
        json fileData = json::object();
        fileData["main"] = json::array();
        fileData["perfect"] = json::array();
        fileData["5e"] = json::array();
        fileData["normal"] = json::array();

        // 隔离存储账号
        if (j.is_array()) {
            for (const auto& item : j) {
                std::string cat = item.value("category", "normal");
                if (cat == "main") {
                    fileData["main"].push_back(item);
                } else if (cat == "perfect") {
                    fileData["perfect"].push_back(item);
                } else if (cat == "5e") {
                    fileData["5e"].push_back(item);
                } else {
                    fileData["normal"].push_back(item);
                }
            }
        }

        std::filesystem::path path = GetAccountsFilePath();
        // 显式以 UTF-8 写入
        std::ofstream file(path.wstring(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (file.is_open()) {
            std::string content = fileData.dump(4);
            file.write(content.c_str(), content.length());
            file.close();
        }
    }
    catch (const std::exception& e) {
        std::wstring err = L"JSON Save Error: " + StringToWString(e.what());
        // 转义内容：“错误”
        MessageBoxW(NULL, err.c_str(), L"\u9519\u8bef", MB_OK);
    }
}
// 主窗口
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 解决高分屏模糊：开启进程 DPI 感知
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // WebView2 依赖于 COM，必须初始化 COM 环境
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        // 使用转义字符表示：“COM 初始化失败”，“错误”
        MessageBoxW(nullptr, L"COM \u521d\u59cb\u5316\u5931\u8d25", L"\u9519\u8bef", MB_OK);
        return FALSE;
    }

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_STEAMACCOUNTMANAGER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_STEAMACCOUNTMANAGER));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CoUninitialize();

    return (int) msg.wParam;
}

// 注册窗口类
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_STEAMACCOUNTMANAGER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = CreateSolidBrush(RGB(30, 30, 30)); // 深色主题背景 #1e1e1e
    wcex.lpszMenuName   = NULL; // Remove menu
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}
// 初始化窗口
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   // 加载主题设置
   json initConfig = ReadConfigJson();
   if (initConfig.contains("theme")) {
       std::string theme = initConfig.value("theme", "dark");
       g_isDarkTheme = (theme != "light");
   }

   // 1. 使用 WS_POPUP | WS_THICKFRAME 实现无边框 + 可缩放
   // WS_EX_APPWINDOW 确保在任务栏显示图标
   HWND hWnd = CreateWindowExW(WS_EX_APPWINDOW, szWindowClass, szTitle, 
      WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
      CW_USEDEFAULT, 0, w, h, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd) return FALSE;

   // 2. 启用 DWM 阴影 (WS_POPUP 默认没阴影，需要手动开启)
   const MARGINS shadow_margins = { 1, 1, 1, 1 };
   DwmExtendFrameIntoClientArea(hWnd, &shadow_margins);

   // 获取程序目录
   WCHAR path[MAX_PATH];
   GetModuleFileNameW(NULL, path, MAX_PATH);
   std::filesystem::path exePath(path);
   std::wstring rootDir = exePath.parent_path().wstring();

   // 注册内联资源 Scheme 来处理 res:// 或者将 webview 拦截来处理内部资源
   // 不过为了方便，最快的方法仍然是将打包出来的那个单 HTML 文件在运行时写入到 Temp 目录再加载。
   
   // 为了单 EXE 发布，将 WebView2 的用户数据目录重定向到系统的临时目录 (Temp)
   WCHAR tempPath[MAX_PATH];
   GetTempPathW(MAX_PATH, tempPath);
   std::wstring userDataFolder = std::wstring(tempPath) + L"SteamAccountManager_WebView2";

   // 释放内嵌的 HTML 到临时目录
   std::wstring htmlPath = userDataFolder + L"\\index.html";
   
   // 尝试从不同的资源类型名称加载
   HRSRC hResInfo = FindResourceW(hInstance, MAKEINTRESOURCEW(IDR_HTML1), L"HTML");
   if (!hResInfo) {
       hResInfo = FindResourceW(hInstance, MAKEINTRESOURCEW(IDR_HTML1), RT_HTML);
   }

   if (hResInfo) {
       HGLOBAL hResData = LoadResource(hInstance, hResInfo);
       if (hResData) {
           DWORD resSize = SizeofResource(hInstance, hResInfo);
           void* pData = LockResource(hResData);
           if (pData) {
               // 确保目录创建成功（如果是多级目录可能需要分别创建）
               CreateDirectoryW(userDataFolder.c_str(), NULL);
               
               HANDLE hFile = CreateFileW(htmlPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
               if (hFile != INVALID_HANDLE_VALUE) {
                   DWORD bytesWritten;
                   WriteFile(hFile, pData, resSize, &bytesWritten, NULL);
                   CloseHandle(hFile);
               } else {
                   MessageBoxW(NULL, L"无法释放 index.html 到 Temp 目录", L"文件写入错误", MB_OK);
                   return 0;
               }
           }
       } else {
           MessageBoxW(NULL, L"无法加载 HTML 资源", L"资源错误", MB_OK);
           return 0;
       }
   } else {
       MessageBoxW(NULL, L"找不到 IDR_HTML1 资源", L"资源错误", MB_OK);
       return 0;
   }

   // 初始化 WebView2
   HRESULT res = CreateCoreWebView2EnvironmentWithOptions(nullptr, userDataFolder.c_str(), nullptr,
       Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
           [hWnd, rootDir, userDataFolder](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
               if (FAILED(result)) {
                   // 转义内容：“WebView2 环境创建失败: ”，“错误”
                   MessageBoxW(hWnd, (L"WebView2 \u73af\u5883\u521b\u5efa\u5931\u8d25: " + std::to_wstring(result)).c_str(), L"\u9519\u8bef", MB_OK);
                   return result;
               }

               env->CreateCoreWebView2Controller(hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                   [hWnd, rootDir, userDataFolder](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                       if (FAILED(result) || controller == nullptr) {
                           // 转义内容：“WebView2 控制器创建失败: ”，“错误”
                           MessageBoxW(hWnd, (L"WebView2 \u63a7\u5236\u5668\u521b\u5efa\u5931\u8d25: " + std::to_wstring(result)).c_str(), L"\u9519\u8bef", MB_OK);
                           return result;
                       }

                       webviewController = controller;
                       webviewController->get_CoreWebView2(&webview);

                       // 彻底解决消息穿透问题：遍历并子类化 WebView2 下的所有子窗口
                       EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hWnd);

                       // 配置 WebView2
                       ICoreWebView2Settings* Settings;
                       webview->get_Settings(&Settings);
                       Settings->put_IsScriptEnabled(TRUE);
                       Settings->put_AreDefaultContextMenusEnabled(FALSE);
                       Settings->put_IsWebMessageEnabled(TRUE);

                       // 1. 拦截所有新窗口请求，改用系统默认浏览器打开
                       webview->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                           [](ICoreWebView2* sender, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                               wil::unique_cotaskmem_string uri;
                               args->get_Uri(&uri);
                               ShellExecuteW(NULL, L"open", uri.get(), NULL, NULL, SW_SHOWNORMAL);
                               args->put_Handled(TRUE); // 禁止 WebView 内部打开新窗口
                               return S_OK;
                           }).Get(), nullptr);

                       // 移除之前的禁用滚动手脚...

                       // 设置初始大小并显示 (透明边框模式，填满整个窗口)
                       RECT rc;
                       GetClientRect(hWnd, &rc);
                       int border = 0; // 透明边框模式：WebView2 填满整个窗口
                       RECT webviewRect = {
                           border,
                           border,
                           rc.right - border,
                           rc.bottom - border
                       };
                       webviewController->put_Bounds(webviewRect);
                       webviewController->put_IsVisible(TRUE);

                       // 映射虚拟域名到本地目录，解决 file:/// 协议的各类限制
                       wil::com_ptr<ICoreWebView2_3> webview3;
                       if (SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview3)))) {
                           webview3->SetVirtualHostNameToFolderMapping(
                               L"app.assets", 
                               userDataFolder.c_str(), 
                               COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW
                           );
                       }

                       // 注册消息处理
                       webview->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                           [hWnd](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                               wil::unique_cotaskmem_string messageRaw;
                               // 重要：前端发送的是对象，这里必须使用 get_WebMessageAsJson
                               args->get_WebMessageAsJson(&messageRaw);
                               if (!messageRaw) return S_OK;

                               try {
                                   std::wstring wmsg(messageRaw.get());
                                   std::string msgUtf8 = WStringToString(wmsg);
                                   json j = json::parse(msgUtf8);

                                   std::string type = j.value("type", "");

                                   // 1. 处理获取账号请求
                                   if (type == "getAccounts") {
                                       json accounts = ReadAccountsJson();
                                       json response = {
                                           {"type", "setAccounts"},
                                           {"accounts", accounts}
                                       };
                                       std::wstring wResponse = StringToWString(response.dump());
                                       webview->PostWebMessageAsJson(wResponse.c_str());
                                   }
                                   // 2. 处理保存账号请求
                                   else if (type == "saveAccounts") {
                                       if (j.contains("accounts")) {
                                           SaveAccountsJson(j["accounts"]);
                                       }
                                   }
                                   // 2.5 处理获取配置请求
                                   else if (type == "getConfig") {
                                       json config = ReadConfigJson();
                                       json response = {
                                           {"type", "setConfig"},
                                           {"config", config}
                                       };
                                       std::wstring wResponse = StringToWString(response.dump());
                                       webview->PostWebMessageAsJson(wResponse.c_str());
                                   }
                                   // 2.6 处理保存配置请求
                                   else if (type == "saveConfig") {
                                       if (j.contains("config")) {
                                           SaveConfigJson(j["config"]);
                                           // 更新主题设置并重绘窗口边框
                                           if (j["config"].contains("theme")) {
                                               std::string theme = j["config"]["theme"].get<std::string>();
                                               bool newIsDark = (theme != "light");
                                               if (g_isDarkTheme != newIsDark) {
                                                   g_isDarkTheme = newIsDark;
                                                   // 重绘窗口边框
                                                   RECT rc;
                                                   GetClientRect(hWnd, &rc);
                                                   InvalidateRect(hWnd, &rc, TRUE);
                                               }
                                           }
                                       }
                                   }
                                   // 3. 处理登录请求
                                   else if (type == "login") {
                                       std::string username = j.value("username", "");
                                       std::string password = j.value("password", "");
                                       
                                       if (!username.empty()) {
                                           std::wstring wUser = StringToWString(username);
                                           std::wstring wPass = StringToWString(password);
                                           
                                           // 调试输出：检查是否收到了密码 (仅调试控制台可见)
                                           OutputDebugStringW((L"[Debug] Login User: " + wUser + L" Pass Length: " + std::to_wstring(wPass.length()) + L"\n").c_str());

                                           // 开启后台线程执行登录序列
                                           std::thread([wUser, wPass]() {
                                               LARGE_INTEGER freq, start, end, stepStart, stepEnd;
                                               QueryPerformanceFrequency(&freq);
                                               QueryPerformanceCounter(&start);
                                               stepStart = start;

                                               KillSteam();
                                               QueryPerformanceCounter(&stepEnd);
                                               OutputDebugStringW((L"[Debug] KillSteam: " + std::to_wstring((double)(stepEnd.QuadPart - stepStart.QuadPart) / freq.QuadPart * 1000.0) + L" ms\n").c_str());
                                               stepStart = stepEnd;

                                               SetAutoLoginUser(wUser);
                                               QueryPerformanceCounter(&stepEnd);
                                               OutputDebugStringW((L"[Debug] SetAutoLoginUser: " + std::to_wstring((double)(stepEnd.QuadPart - stepStart.QuadPart) / freq.QuadPart * 1000.0) + L" ms\n").c_str());
                                               stepStart = stepEnd;

                                               StartSteam(wUser, wPass);
                                               QueryPerformanceCounter(&end);
                                               OutputDebugStringW((L"[Debug] StartSteam: " + std::to_wstring((double)(end.QuadPart - stepStart.QuadPart) / freq.QuadPart * 1000.0) + L" ms\n").c_str());

                                               double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart * 1000.0;
                                               std::wstring timingMsg = L"[Debug] Login completed in " + std::to_wstring(elapsed) + L" ms\n";
                                               OutputDebugStringW(timingMsg.c_str());
                                           }).detach();
                                       }
                                   }
                                   // 4. 处理打开外部链接请求
                                   else if (type == "openUrl") {
                                       std::string url = j.value("url", "");
                                       if (!url.empty()) {
                                           ShellExecuteW(NULL, L"open", StringToWString(url).c_str(), NULL, NULL, SW_SHOWNORMAL);
                                       }
                                   }
                                   // 5. 窗口控制指令
                                   else if (type == "windowControl") {
                                       std::string action = j.value("action", "");
                                       if (action == "minimize") {
                                           ShowWindow(hWnd, SW_MINIMIZE);
                                       }
                                       else if (action == "maximize") {
                                           if (IsZoomed(hWnd)) ShowWindow(hWnd, SW_RESTORE);
                                           else ShowWindow(hWnd, SW_MAXIMIZE);
                                       }
                                       else if (action == "close") {
                                           PostMessage(hWnd, WM_CLOSE, 0, 0);
                                       }
                                       else if (action == "startDrag") {
                                           // 核心：释放捕捉并发送 HTCAPTION 指令实现拖动
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                                       }
                                       // 窗口缩放控制
                                       else if (action == "startResizeLeft") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTLEFT, 0);
                                       }
                                       else if (action == "startResizeRight") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTRIGHT, 0);
                                       }
                                       else if (action == "startResizeTop") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTTOP, 0);
                                       }
                                       else if (action == "startResizeBottom") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTBOTTOM, 0);
                                       }
                                       else if (action == "startResizeTopLeft") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTTOPLEFT, 0);
                                       }
                                       else if (action == "startResizeTopRight") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTTOPRIGHT, 0);
                                       }
                                       else if (action == "startResizeBottomLeft") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTBOTTOMLEFT, 0);
                                       }
                                       else if (action == "startResizeBottomRight") {
                                           ReleaseCapture();
                                           SendMessage(hWnd, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
                                       }
                                   }
                               }
                               catch (const std::exception& e) {
                                   OutputDebugStringA(e.what());
                               }

                               return S_OK;
                           }).Get(), nullptr);

                       // 导航到页面 (使用资源协议加载内部打包的 html)
                       // 映射虚拟域名到虚拟的资源路径
                       webview->Navigate(L"https://app.assets/index.html");

                       return S_OK;
                   }).Get());
               return S_OK;
           }).Get());

   if (FAILED(res)) {
       // 转义内容：“WebView2 初始化调用失败，请检查是否安装了 WebView2 Runtime”，“错误”
       MessageBoxW(hWnd, L"WebView2 \u521d\u59cb\u5316\u8c03\u7528\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u662f\u5426\u5b89\u88c5\u4e86 WebView2 Runtime", L"\u9519\u8bef", MB_OK);
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}
// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // 处理窗口消息
    switch (message)
    {
    //  绘制窗口背景（透明边框版本）
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        
        // 根据主题选择背景颜色（匹配 Vue CSS 变量）
        COLORREF backColor = g_isDarkTheme ? RGB(30, 30, 30) : RGB(243, 243, 243);    // 深色: #1e1e1e, 浅色: #f3f3f3
        HBRUSH hBackBrush = CreateSolidBrush(backColor);
        
        // 填充整个客户区为背景色，确保在WebView2加载前显示统一背景
        FillRect(hdc, &rc, hBackBrush);
        
        DeleteObject(hBackBrush);
        return 1;
    }
    // 处理鼠标移动消息
    case WM_MOUSEMOVE:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc;
        GetClientRect(hWnd, &rc);

        int windowControlsLeft = rc.right - 140;
        bool isOverControls = (pt.x >= windowControlsLeft && pt.y <= 32);

        if (isOverControls != g_isHoveringWindowControls) {
            g_isHoveringWindowControls = isOverControls;
            RECT hoverRect = { rc.right - 140, 0, rc.right, 4 };
            InvalidateRect(hWnd, &hoverRect, FALSE);
        }

        if (!g_isTrackingMouse) {
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            g_isTrackingMouse = true;
        }
        break;
    }
    // 处理鼠标离开消息
    case WM_MOUSELEAVE:
    {
        g_isTrackingMouse = false;
        if (g_isHoveringWindowControls) {
            g_isHoveringWindowControls = false;
            RECT rc;
            GetClientRect(hWnd, &rc);
            RECT hoverRect = { rc.right - 140, 0, rc.right, 4 };
            InvalidateRect(hWnd, &hoverRect, FALSE);
        }
        break;
    }
    // 处理窗口激活消息
    case WM_NCACTIVATE:
        // 阻止系统在激活/非激活时绘制默认标题栏/边框
        return TRUE;
    // 处理鼠标双击消息
    case WM_LBUTTONDBLCLK:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        // 注意：WM_LBUTTONDBLCLK 的坐标是 client 坐标，无需转换
        RECT rc;
        GetClientRect(hWnd, &rc);
        const int titleHeight = 32;

        // 如果双击在标题栏区域（且避开了右侧控制按钮），执行最大化/还原
        if (pt.y < titleHeight && pt.x < rc.right - 140) {
            if (IsZoomed(hWnd)) ShowWindow(hWnd, SW_RESTORE);
            else ShowWindow(hWnd, SW_MAXIMIZE);
            return 0;
        }
        break;
    }
    // 处理窗口大小调整消息
    case WM_NCCALCSIZE:
        // WS_POPUP 模式下不需要特殊处理 NC 区域，直接返回 0 即可
        return 0;
    // 处理窗口大小调整消息
    case WM_NCHITTEST:
    {
        // 前端检测鼠标位置并处理窗口缩放，此处始终返回HTCLIENT让前端处理
        return HTCLIENT;
    }
    // 处理窗口大小调整消息
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        // 设置最小可拖拽尺寸
        lpMMI->ptMinTrackSize.x = 1100;
        lpMMI->ptMinTrackSize.y = 830;
        
        // 关键：处理 WS_POPUP 模式下的最大化溢出
        // 获取窗口所在的当前显示器
        HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        if (hMonitor != NULL) {
            MONITORINFO mi = { sizeof(mi) };
            if (GetMonitorInfo(hMonitor, &mi)) {
                // ptMaxPosition 是相对于系统主显示器 (0,0) 的偏移
                // 所以不需要减去 rcMonitor.left，只需计算屏幕左上角相对工作区左上角的偏移
                // 在多显示器且不同分辨率下，正确设置偏移非常重要
                lpMMI->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                lpMMI->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                
                // 设置最大化时的宽高为工作区 (rcWork) 的宽高
                lpMMI->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                lpMMI->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
                
                // 限制最大可拖拽范围
                lpMMI->ptMaxTrackSize.x = lpMMI->ptMaxSize.x;
                lpMMI->ptMaxTrackSize.y = lpMMI->ptMaxSize.y;
            }
        }
        return 0; // 必须返回 0 表示已处理
    }
    // 处理窗口大小调整消息
    case WM_SIZE:
        if (webviewController != nullptr) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            // 透明边框模式：WebView2 填满整个窗口，缩放通过 WM_NCHITTEST 处理
            int border = 0;
            
            RECT webviewRect = {
                border,
                border,
                rc.right - border,
                rc.bottom - border
            };
            webviewController->put_Bounds(webviewRect);
            
            // 关键：重新遍历子窗口
            EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hWnd);
        }
        break;
    // 处理进入缩放/移动模式
    case WM_ENTERSIZEMOVE:
        // 缩放开始时重新子类化所有子窗口
        if (webviewController != nullptr) {
            EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hWnd);
        }
        break;
    // 处理退出缩放/移动模式
    case WM_EXITSIZEMOVE:
        // 缩放结束后重新子类化所有子窗口
        if (webviewController != nullptr) {
            EnumChildWindows(hWnd, EnumChildProc, (LPARAM)hWnd);
        }
        break;
    // 处理窗口销毁消息
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
