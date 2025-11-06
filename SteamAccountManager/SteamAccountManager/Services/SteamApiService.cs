// Services/SteamApiService.cs
using Newtonsoft.Json;
using SteamAccountManager.Models;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Threading.Tasks;
using System.Windows;

namespace SteamAccountManager.Services
{
    public class SteamApiService
    {
        private readonly HttpClient _httpClient;

        public SteamApiService()
        {
            _httpClient = new HttpClient();
        }

        // API: 获取玩家摘要信息 (昵称, 头像等)
        public async Task<List<PlayerSummary>> GetPlayerSummariesAsync(string apiKey, IEnumerable<string> steamIds)
        {
            if (string.IsNullOrEmpty(apiKey) || !steamIds.Any()) return new List<PlayerSummary>();

            var steamIdsString = string.Join(",", steamIds);
            var url = $"https://api.steampowered.com/ISteamUser/GetPlayerSummaries/v2/?key={apiKey}&steamids={steamIdsString}";

            try
            {
                var response = await _httpClient.GetStringAsync(url);
                var result = JsonConvert.DeserializeObject<PlayerSummaryResponse>(response);
                return result?.Response?.Players ?? new List<PlayerSummary>();
            }
            catch (System.Exception ex)
            {
                MessageBox.Show($"API 请求失败 (GetPlayerSummaries): {ex.Message}", "API 错误", MessageBoxButton.OK, MessageBoxImage.Error);
                return new List<PlayerSummary>();
            }
        }

        // API: 获取玩家封禁信息
        public async Task<List<PlayerBans>> GetPlayerBansAsync(string apiKey, IEnumerable<string> steamIds)
        {
            if (string.IsNullOrEmpty(apiKey) || !steamIds.Any()) return new List<PlayerBans>();

            var steamIdsString = string.Join(",", steamIds);
            var url = $"https://api.steampowered.com/ISteamUser/GetPlayerBans/v1/?key={apiKey}&steamids={steamIdsString}";

            try
            {
                var response = await _httpClient.GetStringAsync(url);
                var result = JsonConvert.DeserializeObject<PlayerBansResponse>(response);
                return result?.Players ?? new List<PlayerBans>();
            }
            catch (System.Exception ex)
            {
                MessageBox.Show($"API 请求失败 (GetPlayerBans): {ex.Message}", "API 错误", MessageBoxButton.OK, MessageBoxImage.Error);
                return new List<PlayerBans>();
            }
        }
    }

    // --- 用于反序列化 API 响应的辅助类 ---

    public class PlayerSummaryResponse { public PlayerSummaryRoot Response { get; set; } }
    public class PlayerSummaryRoot { public List<PlayerSummary> Players { get; set; } }
    public class PlayerSummary
    {
        [JsonProperty("steamid")]
        public string SteamId { get; set; }
        [JsonProperty("personaname")]
        public string PersonaName { get; set; }
    }

    public class PlayerBansResponse { public List<PlayerBans> Players { get; set; } }
    public class PlayerBans
    {
        public string SteamId { get; set; }
        public bool CommunityBanned { get; set; }
        [JsonProperty("VACBanned")]
        public bool VACBanned { get; set; }

        [JsonProperty("NumberOfVACBans")]
        public int NumberOfVACBans { get; set; }

        [JsonProperty("DaysSinceLastBan")]
        public int DaysSinceLastBan { get; set; }

        [JsonProperty("NumberOfGameBans")]
        public int NumberOfGameBans { get; set; }

        // 新增此字段，用于判断竞技冷却
        [JsonProperty("EconomyBan")]
        public string EconomyBan { get; set; } // "none", "probation", "banned"
    }
}