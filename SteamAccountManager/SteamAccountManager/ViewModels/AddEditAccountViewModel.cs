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
                Account = new Account
                {
                    Id = accountToEdit.Id,
                    Username = accountToEdit.Username,
                    Password = accountToEdit.Password,
                    SteamID64 = accountToEdit.SteamID64,
                    Nickname = accountToEdit.Nickname,
                    Email = accountToEdit.Email,
                    EmailPassword = accountToEdit.EmailPassword,

                    Cs2PrimeStatus = accountToEdit.Cs2PrimeStatus,
                    Cs2BanStatus = accountToEdit.Cs2BanStatus,
                    Cs2CooldownExpiry = accountToEdit.Cs2CooldownExpiry,
                    IsVacBanned = accountToEdit.IsVacBanned
                };
                WindowTitle = "编辑账号";
                InitializeCooldownTime(accountToEdit.Cs2CooldownExpiry);
            }

            SaveCommand = new RelayCommand(Save);
        }
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
            if (Account.Cs2BanStatus == "冷却")
            {
                int days = CooldownDays.GetValueOrDefault();
                int hours = CooldownHours.GetValueOrDefault();
                Account.Cs2CooldownExpiry = (days > 0 || hours > 0)
                    ? DateTime.Now.AddDays(days).AddHours(hours)
                    : (DateTime?)null;
            }
            else
            {
                Account.Cs2CooldownExpiry = null;
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