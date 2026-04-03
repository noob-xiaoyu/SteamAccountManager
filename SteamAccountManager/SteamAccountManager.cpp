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

void StartSteam(const std::wstring& username, const std::wstring& password) {
    std::wstring steamPath = GetSteamPath();
    if (!steamPath.empty()) {
        // 获取 Steam 所在的目录作为工作目录
        std::filesystem::path p(steamPath);
        std::wstring steamDir = p.parent_path().wstring();

        // 构建启动参数
        std::wstring args = L" -no-browser -skipinitialbootstrap -nobootstrapupdate -noreactlogin -login " + username + L"";
        
        // 只有在密码不为空时才附加密码参数
        if (!password.empty()) {
            args += L" " + password + L"";
        }

        // 使用 ShellExecuteW 启动，并指定工作目录
        HINSTANCE result = ShellExecuteW(NULL, L"open", steamPath.c_str(), args.c_str(), steamDir.c_str(), SW_SHOWNORMAL);
        
        if ((INT_PTR)result <= 32) {
            ShellExecuteW(NULL, L"open", steamPath.c_str(), NULL, steamDir.c_str(), SW_SHOWNORMAL);
        }
    }
}

void KillSteam() {
    if (!IsSteamRunning()) return;

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
                    // 等待进程退出，最多等待 5 秒
                    WaitForSingleObject(hProc, 5000);
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
    wcex.hbrBackground  = CreateSolidBrush(RGB(45, 45, 48)); // 设为和 Vue 相同的背景色 #2D2D30
    wcex.lpszMenuName   = NULL; // Remove menu
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

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
               }
           }
       } else {
           MessageBoxW(NULL, L"无法加载 HTML 资源", L"资源错误", MB_OK);
       }
   } else {
       MessageBoxW(NULL, L"找不到 IDR_HTML1 资源", L"资源错误", MB_OK);
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

                       // 设置初始大小并显示 (留出 4 像素边框以优化缩放感应)
                       RECT rc;
                       GetClientRect(hWnd, &rc);
                       int border = 4;
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
                                               KillSteam();
                                               Sleep(2000); 
                                               SetAutoLoginUser(wUser);
                                               StartSteam(wUser, wPass);
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH hBackBrush = CreateSolidBrush(RGB(45, 45, 48));
    static HBRUSH hStatusBrush = CreateSolidBrush(RGB(0, 122, 204)); // #007ACC

    switch (message)
    {
    case WM_ERASEBKGND:
    {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        int border = 4;
        int statusBarHeight = 31; // Vue 状态栏的大致高度

        // 1. 填充顶部和主背景上方的左右感应区 (RGB(45, 45, 48))
        RECT topRect = { rc.left, rc.top, rc.right, border };
        RECT leftMainRect = { rc.left, border, border, rc.bottom - border - statusBarHeight };
        RECT rightMainRect = { rc.right - border, border, rc.right, rc.bottom - border - statusBarHeight };
        
        FillRect(hdc, &topRect, hBackBrush);
        FillRect(hdc, &leftMainRect, hBackBrush);
        FillRect(hdc, &rightMainRect, hBackBrush);

        // 2. 填充底部和状态栏层级的左右感应区 (RGB(0, 122, 204))
        // 这将使底部的蓝色条看起来完全贯穿左右
        RECT leftStatusRect = { rc.left, rc.bottom - border - statusBarHeight, border, rc.bottom - border };
        RECT rightStatusRect = { rc.right - border, rc.bottom - border - statusBarHeight, rc.right, rc.bottom - border };
        RECT bottomRect = { rc.left, rc.bottom - border, rc.right, rc.bottom };

        FillRect(hdc, &leftStatusRect, hStatusBrush);
        FillRect(hdc, &rightStatusRect, hStatusBrush);
        FillRect(hdc, &bottomRect, hStatusBrush);

        return 1;
    }

    case WM_NCACTIVATE:
        // 阻止系统在激活/非激活时绘制默认标题栏/边框
        return TRUE;

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

    case WM_NCCALCSIZE:
        // WS_POPUP 模式下不需要特殊处理 NC 区域，直接返回 0 即可
        return 0;

    case WM_NCHITTEST:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        RECT rc;
        GetClientRect(hWnd, &rc);

        int border = 4; // 缩放感应区

        // 判定四个角
        if (pt.x < border && pt.y < border) return HTTOPLEFT;
        if (pt.x > rc.right - border && pt.y < border) return HTTOPRIGHT;
        if (pt.x < border && pt.y > rc.bottom - border) return HTBOTTOMLEFT;
        if (pt.x > rc.right - border && pt.y > rc.bottom - border) return HTBOTTOMRIGHT;

        // 判定四条边
        if (pt.x < border) return HTLEFT;
        if (pt.x > rc.right - border) return HTRIGHT;
        if (pt.y < border) return HTTOP;
        if (pt.y > rc.bottom - border) return HTBOTTOM;

        return HTCLIENT;
    }

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
    case WM_SIZE:
        if (webviewController != nullptr) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            
            // 如果窗口已最大化，则 WebView2 填满整个窗口 (不需要 4 像素感应区)
            // 如果是常规状态，保留 4 像素边距用于缩放
            int border = IsZoomed(hWnd) ? 0 : 4;
            
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
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
