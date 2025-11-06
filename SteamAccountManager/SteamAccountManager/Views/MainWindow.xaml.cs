// MainWindow.xaml.cs
using SteamAccountManager.Models;
using SteamAccountManager.ViewModels;
using System.Diagnostics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using System.Windows.Input;

namespace SteamAccountManager
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            // 绑定 DataGrid 的选择变化事件
            this.Loaded += MainWindow_Loaded;
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            AccountsGrid.SelectionChanged += DataGrid_SelectionChanged;
        }

        private void DataGrid_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (this.DataContext is MainViewModel viewModel)
            {
                // 清空旧的选择
                viewModel.SelectedAccounts.Clear();
                // 添加所有当前选中的项
                foreach (Account item in AccountsGrid.SelectedItems)
                {
                    viewModel.SelectedAccounts.Add(item);
                }
            }
        }
        private void DataGridRow_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            // 确保 DataContext 是我们的 ViewModel
            if (this.DataContext is MainViewModel viewModel)
            {
                // 检查 "更改选中" 命令是否可以执行
                if (viewModel.EditAccountCommand.CanExecute(null))
                {
                    // 直接执行命令
                    viewModel.EditAccountCommand.Execute(null);
                }
            }
        }
        private void Hyperlink_RequestNavigate(object sender, RequestNavigateEventArgs e)
        {
            // 使用默认浏览器打开链接
            Process.Start(new ProcessStartInfo(e.Uri.AbsoluteUri) { UseShellExecute = true });
            e.Handled = true;
        }
    }
}