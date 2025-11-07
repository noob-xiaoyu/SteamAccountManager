// Views/BulkAddWindow.xaml.cs
using SteamAccountManager.Models;
using System;
using System.Collections.Generic;
using System.Text.RegularExpressions; // 引入正则表达式的命名空间
using System.Windows;

namespace SteamAccountManager.Views
{
    public partial class BulkAddWindow : Window
    {
        public List<Account> ParsedAccounts { get; private set; } = new List<Account>();

        public BulkAddWindow()
        {
            InitializeComponent();
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            // 获取所有行
            var lines = AccountsTextBox.Text.Split(new[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
            
            foreach (var line in lines)
            {
                // [核心改动] 在分割前，先清理字符串
                string cleanedLine = CleanUpLine(line);

                var parts = cleanedLine.Split(new[] { "----", "---", "--", "-", ",", "|" }, StringSplitOptions.None);
                
                if (parts.Length >= 2) // 至少要有用户名和密码
                {
                    var account = new Account
                    {
                        Nickname = parts[0].Trim(),
                        Username = parts[0].Trim(),
                        Password = parts[1].Trim(),
                        Email = parts.Length > 2 ? parts[2].Trim() : string.Empty,
                        EmailPassword = parts.Length > 3 ? parts[3].Trim() : string.Empty,
                        EmailUrl = parts.Length > 4 ? parts[4].Trim() : string.Empty
                    };
                    ParsedAccounts.Add(account);
                }
            }

            if (ParsedAccounts.Any())
            {
                this.DialogResult = true;
            }
            else
            {
                MessageBox.Show("没有解析到任何有效的账号信息，请检查格式。\n每行格式应为：用户名----密码----SteamID64(可选)----备注(可选)", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }

        /// <summary>
        /// 使用正则表达式清理单行文本，移除常见的前缀标签。
        /// </summary>
        /// <param name="line">原始输入的行文本。</param>
        /// <returns>清理后的行文本。</returns>
        private string CleanUpLine(string line)
        {
            // 这个正则表达式会匹配行首的常见标签，如 "账号"、"密码"、"steamid" 等，
            // 后面可以跟中文冒号 "：" 或英文冒号 ":"，以及任意数量的空格。
            // (账号|密码|备注|昵称|steamid|steam id|username|password|remarks|nickname)
            // 匹配这些关键字（不区分大小写）
            // [：:]?
            // 匹配一个可选的中文或英文冒号
            // \s*
            // 匹配任意数量的空格
            string pattern = @"^(账号|密码|备注|昵称|邮箱|邮箱密码|邮箱地址|地址|steamid|steam id|username|password|remarks|nickname)\s*[:：]?\s*";

            // Regex.Replace 会查找匹配项并将其替换为空字符串， effectively removing it.
            // RegexOptions.IgnoreCase 使得匹配不区分大小写。
            string cleaned = Regex.Replace(line, pattern, "", RegexOptions.IgnoreCase);

            return cleaned.Trim();
        }
    }
}