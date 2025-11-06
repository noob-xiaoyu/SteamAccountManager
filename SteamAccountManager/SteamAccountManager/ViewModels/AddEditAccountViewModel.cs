// ViewModels/AddEditAccountViewModel.cs
using SteamAccountManager.Helpers;
using SteamAccountManager.Models;
using System;
using System.ComponentModel;
using System.Windows;
using System.Windows.Input;

namespace SteamAccountManager.ViewModels
{
    public class AddEditAccountViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        // --- 核心数据对象 ---
        public Account Account { get; set; }

        // --- UI 绑定属性 ---
        public string WindowTitle { get; set; }

        private int? _cooldownDays;
        public int? CooldownDays
        {
            get => _cooldownDays;
            set { _cooldownDays = value; OnPropertyChanged(nameof(CooldownDays)); }
        }

        private int? _cooldownHours;
        public int? CooldownHours
        {
            get => _cooldownHours;
            set { _cooldownHours = value; OnPropertyChanged(nameof(CooldownHours)); }
        }

        // --- 命令 ---
        public ICommand SaveCommand { get; }

        public AddEditAccountViewModel(Account accountToEdit)
        {
            if (accountToEdit == null)
            {
                // --- 添加新账号模式 ---
                Account = new Account(); // Status 默认为 "未知"
                WindowTitle = "添加新账号";
            }
            else
            {
                // --- 编辑现有账号模式 ---
                // 创建一个副本进行编辑，以支持取消操作
                Account = new Account
                {
                    Id = accountToEdit.Id,
                    Username = accountToEdit.Username,
                    Password = accountToEdit.Password,
                    SteamID64 = accountToEdit.SteamID64,
                    Nickname = accountToEdit.Nickname, // 统一使用 Nickname
                    Status = accountToEdit.Status, // 复制 Status
                    CooldownExpiryDate = accountToEdit.CooldownExpiryDate, // 复制 CooldownExpiryDate
                    IsPrime = accountToEdit.IsPrime,
                    Email = accountToEdit.Email,
                    EmailPassword = accountToEdit.EmailPassword
                };
                WindowTitle = "编辑账号";

                // 根据 CooldownExpiryDate 初始化 CooldownDays 和 CooldownHours
                InitializeCooldownTime(accountToEdit.CooldownExpiryDate);
            }

            SaveCommand = new RelayCommand(Save);
        }

        /// <summary>
        /// 根据现有的冷却截止日期，反向计算出剩余的天数和小时数，用于在 UI 上显示。
        /// </summary>
        private void InitializeCooldownTime(DateTime? expiryDate)
        {
            if (expiryDate.HasValue)
            {
                var timeLeft = expiryDate.Value - DateTime.Now;
                if (timeLeft.TotalSeconds > 0)
                {
                    CooldownDays = timeLeft.Days;
                    CooldownHours = timeLeft.Hours;
                }
            }
        }

        /// <summary>
        /// 当用户点击“确定”按钮时执行的保存逻辑。
        /// </summary>
        private void Save(object parameter)
        {
            // 1. 数据验证
            if (string.IsNullOrWhiteSpace(Account.Username) || string.IsNullOrWhiteSpace(Account.Password))
            {
                MessageBox.Show("账号和密码为必填项。", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }
            if (!string.IsNullOrEmpty(Account.SteamID64) && !long.TryParse(Account.SteamID64, out _))
            {
                MessageBox.Show("SteamID64 格式不正确，应为一串数字。", "提示", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            // 2. 核心逻辑：处理冷却时间
            if (Account.Status == "冷却")
            {
                // 使用 .GetValueOrDefault() 方法，如果 CooldownDays 是 null，它会返回 0
                int days = CooldownDays.GetValueOrDefault();
                int hours = CooldownHours.GetValueOrDefault();

                if (days > 0 || hours > 0)
                {
                    Account.CooldownExpiryDate = DateTime.Now.AddDays(days).AddHours(hours);
                }
                else
                {
                    Account.CooldownExpiryDate = null;
                }
            }
            else
            {
                Account.CooldownExpiryDate = null;
            }

            // 3. 关闭窗口并返回成功
            if (parameter is Window window)
            {
                window.DialogResult = true;
                window.Close();
            }
        }
    }
}