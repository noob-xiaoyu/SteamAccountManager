// Views/StartupNoticeWindow.xaml.cs
using System.Windows;

namespace SteamAccountManager.Views
{
    public partial class StartupNoticeWindow : Window
    {
        public bool DoNotShowAgain => DoNotShowAgainCheckBox.IsChecked == true;

        public StartupNoticeWindow()
        {
            InitializeComponent();
        }

        private void OkButton_Click(object sender, RoutedEventArgs e)
        {
            this.DialogResult = true;
            this.Close();
        }
    }
}