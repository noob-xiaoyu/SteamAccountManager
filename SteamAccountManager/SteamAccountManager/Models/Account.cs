// Models/Account.cs
using Newtonsoft.Json;
using System;
using System.ComponentModel;

namespace SteamAccountManager.Models
{
    // 实现 INotifyPropertyChanged 以便在 DataGrid 中实时更新
    public class Account : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private string _nickname;
        private string _status;
        private bool _isTwoFactorEnabled;
        private string _steamID64;
        private bool _isVacBanned;
        private int _numberOfGameBans;
        private int _daysSinceLastBan;
        private DateTime? _cooldownExpiry;
        private bool _isBanned;
        private bool _isPrime;
        private int _banReason;

        [JsonProperty("Id")]
        public Guid Id { get; set; } = Guid.NewGuid();

        [JsonProperty("Nickname")]
        public string Nickname
        {
            get => _nickname;
            set { _nickname = value; OnPropertyChanged(nameof(Nickname)); }
        }

        [JsonProperty("Username")]
        public string Username { get; set; }

        [JsonProperty("Password")]
        public string Password { get; set; }

        [JsonProperty("SteamId64")]
        public string SteamID64
        {
            get => _steamID64;
            set { _steamID64 = value; OnPropertyChanged(nameof(SteamID64)); }
        }

        [JsonProperty("IsBanned")]
        public bool IsBanned
        {
            get => _isBanned;
            set { _isBanned = value; OnPropertyChanged(nameof(IsBanned)); OnPropertyChanged(nameof(DisplayStatus)); OnPropertyChanged(nameof(SortPriority)); }
        }

        [JsonProperty("BanReason")]
        public int BanReason // 0=正常, 1=冷却, 2=VAC/游戏封禁
        {
            get => _banReason;
            set { _banReason = value; OnPropertyChanged(nameof(BanReason)); OnPropertyChanged(nameof(DisplayStatus)); OnPropertyChanged(nameof(SortPriority)); }
        }

        [JsonProperty("CooldownExpiry")]
        public DateTime? CooldownExpiry
        {
            get => _cooldownExpiry;
            set { _cooldownExpiry = value; OnPropertyChanged(nameof(CooldownExpiry)); OnPropertyChanged(nameof(DisplayStatus)); }
        }

        [JsonProperty("IsPrime")]
        public bool IsPrime
        {
            get => _isPrime;
            set { _isPrime = value; OnPropertyChanged(nameof(IsPrime)); }
        }

        [JsonProperty("Email")]
        public string Email { get; set; }

        [JsonProperty("EmailPassword")]
        public string EmailPassword { get; set; }

        [JsonIgnore] // 这个属性由其他属性计算得出，不需要保存到 JSON
        public string DisplayStatus
        {
            get
            {
                if (!IsBanned) return "正常";

                switch (BanReason)
                {
                    case 1: // 冷却
                        if (CooldownExpiry.HasValue)
                        {
                            var timeLeft = CooldownExpiry.Value - DateTime.Now;
                            if (timeLeft.TotalSeconds > 0)
                            {
                                return timeLeft.TotalDays >= 1
                                    ? $"冷却 (剩 {Math.Ceiling(timeLeft.TotalDays)} 天)"
                                    : $"冷却 (剩 {Math.Ceiling(timeLeft.TotalHours)} 小时)";
                            }
                            return "冷却 (已结束)";
                        }
                        return "冷却 (未设置日期)";
                    case 2: // VAC/游戏封禁
                        return "VAC";
                    default:
                        return "未知封禁";
                }
            }
        }

        [JsonIgnore]
        public string Status
        {
            get
            {
                if (!IsBanned) return "正常";
                switch (BanReason)
                {
                    case 1: return "冷却";
                    case 2: return "VAC";
                    default: return "未知";
                }
            }
            set
            {
                switch (value)
                {
                    case "正常":
                        IsBanned = false;
                        BanReason = 0;
                        CooldownExpiry = null;
                        break;
                    case "冷却":
                        IsBanned = true;
                        BanReason = 1;
                        break;
                    case "VAC":
                        IsBanned = true;
                        BanReason = 2;
                        CooldownExpiry = null;
                        break;
                    default:
                        IsBanned = false;
                        BanReason = 0;
                        break;
                }
                OnPropertyChanged(nameof(Status));
            }
        }
        [JsonIgnore]
        public DateTime? CooldownExpiryDate
        {
            get => CooldownExpiry;
            set => CooldownExpiry = value;
        }
        [JsonIgnore]
        public bool IsVacBanned { get => _isVacBanned; set { _isVacBanned = value; OnPropertyChanged(nameof(IsVacBanned)); } }
        [JsonIgnore]
        public int NumberOfGameBans { get => _numberOfGameBans; set { _numberOfGameBans = value; OnPropertyChanged(nameof(NumberOfGameBans)); } }
        [JsonIgnore]
        public int DaysSinceLastBan { get => _daysSinceLastBan; set { _daysSinceLastBan = value; OnPropertyChanged(nameof(DaysSinceLastBan)); } }

        [JsonIgnore] // 不需要保存到文件
        public int SortPriority
        {
            get
            {
                // 根据需求 "未知->0, 正常->1, 冷却->2, VAC->3"
                // 数字小的排在前面 (Ascending)
                if (!IsBanned) return 1; // 正常

                switch (BanReason)
                {
                    case 1: return 2; // 冷却
                    case 2: return 3; // VAC
                    default: return 0; // 未知
                }
            }
        }
        public Account()
        {
            // 为新字段提供默认值
            Status = "未知";
            SteamID64 = string.Empty;
        }
    }
}