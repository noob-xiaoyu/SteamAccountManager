// App.xaml.cs
using SteamAccountManager.Models;
using SteamAccountManager.Services;
using SteamAccountManager.Views;
using System.Windows;

namespace SteamAccountManager
{
    public partial class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            var dataService = new JsonDataService();
            var settings = dataService.LoadSettings();
            var mainWindow = new MainWindow();
            mainWindow.Show();
            if (settings.ShowStartupNotice)
            {
                var noticeWindow = new StartupNoticeWindow();
                if (noticeWindow.ShowDialog() == true)
                {
                    if (noticeWindow.DoNotShowAgain)
                    {
                        settings.ShowStartupNotice = false;
                        dataService.SaveSettings(settings);
                    }
                }
            }
        }
    }
}