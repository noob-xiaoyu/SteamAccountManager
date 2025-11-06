// Services/JsonDataService.cs
using Newtonsoft.Json;
using SteamAccountManager.Models;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace SteamAccountManager.Services
{
    public class JsonDataService
    {
        private readonly string _accountsFilePath = "accounts.json";
        private readonly string _settingsFilePath = "settings.json";

        // --- Account Methods ---
        public List<Account> LoadAccounts()
        {
            if (!File.Exists(_accountsFilePath))
            {
                return new List<Account>();
            }
            var json = File.ReadAllText(_accountsFilePath);
            return JsonConvert.DeserializeObject<List<Account>>(json) ?? new List<Account>();
        }

        public void SaveAccounts(IEnumerable<Account> accounts)
        {
            var json = JsonConvert.SerializeObject(accounts, Formatting.Indented);
            File.WriteAllText(_accountsFilePath, json);
        }

        // --- Settings Methods ---
        public AppSettings LoadSettings()
        {
            if (!File.Exists(_settingsFilePath))
            {
                return new AppSettings();
            }
            var json = File.ReadAllText(_settingsFilePath);
            return JsonConvert.DeserializeObject<AppSettings>(json) ?? new AppSettings();
        }

        public void SaveSettings(AppSettings settings)
        {
            var json = JsonConvert.SerializeObject(settings, Formatting.Indented);
            File.WriteAllText(_settingsFilePath, json);
        }
    }
}