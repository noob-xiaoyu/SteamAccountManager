// Helpers/SteamLauncher.cs
using Microsoft.Win32;
using SteamAccountManager.Models;
using System.Diagnostics;
using System.Windows;

namespace SteamAccountManager.Helpers
{
    public static class SteamLauncher
    {
        public static void Login(Account account)
        {
            if (account == null) return;

            string steamExePath = GetSteamExePath();
            if (string.IsNullOrEmpty(steamExePath))
            {
                MessageBox.Show("未找到 Steam 安装路径。请确保 Steam 已正确安装。", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            // 确保 Steam 进程已关闭，以允许切换账号
            foreach (var process in Process.GetProcessesByName("steam"))
            {
                process.Kill();
                process.WaitForExit();
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = steamExePath,
                Arguments = $"-login {account.Username} {account.Password}"
            };

            try
            {
                Process.Start(startInfo);
            }
            catch (System.Exception ex)
            {
                MessageBox.Show($"启动 Steam 失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private static string GetSteamExePath()
        {
            // 从注册表读取 Steam 路径
            try
            {
                using (var key = Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam"))
                {
                    return key?.GetValue("SteamExe")?.ToString();
                }
            }
            catch
            {
                return null;
            }
        }
    }
}