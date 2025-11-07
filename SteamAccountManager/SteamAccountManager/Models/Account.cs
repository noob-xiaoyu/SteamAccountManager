// Models/Account.cs
using Newtonsoft.Json;
using System;
using System.ComponentModel;

namespace SteamAccountManager.Models
{
    public class Account : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        // --- 基础信息 ---
        [JsonProperty("Id")]
        public Guid Id { get; set; } = Guid.NewGuid();
        [JsonProperty("Username")]
        public string Username { get; set; }
        [JsonProperty("Password")]
        public string Password { get; set; }
        [JsonProperty("Nickname")]
        public string Nickname { get; set; }
        [JsonProperty("SteamId64")]
        public string SteamID64 { get; set; }
        [JsonProperty("Email")]
        public string Email { get; set; }
        [JsonProperty("EmailPassword")]
        public string EmailPassword { get; set; }
        [JsonProperty("EmailUrl")]
        public string EmailUrl { get; set; }

        // --- CS2 相关状态字段 ---
        private string _cs2PrimeStatus = "需升级⚠"; // "是", "否", "需升级"
        private string _cs2BanStatus = "正常"; // "正常", "冷却", "游戏封禁", "VAC"
        private DateTime? _cs2CooldownExpiry;

        [JsonProperty("Cs2PrimeStatus")]
        public string Cs2PrimeStatus
        {
            get => _cs2PrimeStatus;
            set { _cs2PrimeStatus = value; OnPropertyChanged(nameof(Cs2PrimeStatus)); OnPropertyChanged(nameof(SortPriority)); }
        }

        [JsonProperty("Cs2BanStatus")]
        public string Cs2BanStatus
        {
            get => _cs2BanStatus;
            set
            {
                _cs2BanStatus = value;
                if (_cs2BanStatus != "冷却")
                {
                    Cs2CooldownExpiry = null; // 如果不是冷却，清除日期
                }
                OnPropertyChanged(nameof(Cs2BanStatus));
                OnPropertyChanged(nameof(DisplayStatus));
                OnPropertyChanged(nameof(SortPriority));

            }
        }

        [JsonProperty("Cs2CooldownExpiry")]
        public DateTime? Cs2CooldownExpiry
        {
            get => _cs2CooldownExpiry;
            set { _cs2CooldownExpiry = value; OnPropertyChanged(nameof(Cs2CooldownExpiry)); OnPropertyChanged(nameof(DisplayStatus)); }
        }

        // --- 通用 VAC 状态 ---
        private bool _isVacBanned;
        [JsonProperty("IsVacBanned")]
        public bool IsVacBanned
        {
            get => _isVacBanned;
            set { _isVacBanned = value; OnPropertyChanged(nameof(IsVacBanned)); }
        }

        // --- UI 显示与排序辅助属性 ---
        [JsonIgnore]
        public string DisplayStatus
        {
            get
            {
                if (Cs2BanStatus == "冷却")
                {
                    if (Cs2CooldownExpiry.HasValue)
                    {
                        var timeLeft = Cs2CooldownExpiry.Value - DateTime.Now;
                        return timeLeft.TotalSeconds > 0
                            ? $"冷却 (剩 {Math.Ceiling(timeLeft.TotalDays)} 天)"
                            : "冷却 (已结束)";
                    }
                    return "冷却 (未设置)";
                }
                return Cs2BanStatus;
            }
        }

        [JsonIgnore]
        public int SortPriority
        {
            get
            {
                // 排序: Prime优先, 风险等级其次
                int primeScore = Cs2PrimeStatus == "是" ? 0 : 1;

                int banScore;
                switch (Cs2BanStatus)
                {
                    case "正常": banScore = 1; break;
                    case "冷却": banScore = 2; break;
                    case "游戏封禁": banScore = 3; break;
                    case "VAC": banScore = 4; break;
                    default: banScore = 0; break; // 未知
                }
                // 组合分数，Prime 权重更高
                return primeScore * 10 + banScore;
            }
        }
    }
}