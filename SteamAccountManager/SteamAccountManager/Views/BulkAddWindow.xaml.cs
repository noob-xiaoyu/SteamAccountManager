// Views/BulkAddWindow.xaml.cs
using SteamAccountManager.Models;
using System;
using System.Collections.Generic;
using System.Linq; // 需要引入 Linq
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
            var lines = AccountsTextBox.Text.Split(new[] { Environment.NewLine }, StringSplitOptions.RemoveEmptyEntries);
            foreach (var line in lines)
            {
                // 使用 "----" 分割，最多只分割前两项，后面的保留
                var parts = line.Split(new[] { "----" , "---" , "--" , "-" , "," , "|"}, StringSplitOptions.None);

                // 至少要有用户名和密码
                if (parts.Length >= 2)
                {
                    var account = new Account
                    {
                        Username = parts[0].Trim(),
                        Password = parts[1].Trim(),
                    };

                    // 如果有多于2个部分，将后面的所有部分重新用 "----" 连接起来，作为备注
                    if (parts.Length > 2)
                    {
                        account.Nickname = string.Join("----", parts.Skip(2).Select(p => p.Trim()));
                    }
                    else
                    {
                        // 如果没有其他信息，备注可以为空或默认为用户名
                        account.Nickname = string.Empty;
                    }

                    ParsedAccounts.Add(account);
                }
            }

            if (ParsedAccounts.Any())
            {
                this.DialogResult = true;
            }
            else
            {
                MessageBox.Show("没有解析到任何有效的账号信息，请检查格式（至少需要 用户名----密码）。", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
            }
        }
    }
}