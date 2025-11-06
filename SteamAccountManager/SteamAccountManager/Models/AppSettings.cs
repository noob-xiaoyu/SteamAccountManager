// Models/AppSettings.cs
namespace SteamAccountManager.Models
{
    public class AppSettings
    {
        public bool ShowStartupNotice { get; set; } = true;
        public string ApiKey { get; set; }
    }
}