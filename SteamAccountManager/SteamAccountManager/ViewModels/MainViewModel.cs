// ViewModels/MainViewModel.cs
using SteamAccountManager.Helpers;
using SteamAccountManager.Models;
using SteamAccountManager.Services;
using SteamAccountManager.Views;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Threading;
using System.Collections.Generic;

namespace SteamAccountManager.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {

        // --- 核心数据与UI绑定属性 ---
        public Account Account { get; set; }
        public bool IsCooldownControlsVisible => Account?.Cs2BanStatus == "冷却";
        public ObservableCollection<Account> Accounts { get; set; }
        public ICollectionView AccountsView { get; private set; }
        private DispatcherTimer _cooldownCheckTimer;
        public event PropertyChangedEventHandler PropertyChanged;
        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        // --- 服务依赖 ---
        private readonly JsonDataService _dataService;
        private readonly SteamApiService _apiService;
        private string _statusText = "就绪";
        public string StatusText { get => _statusText; set { _statusText = value; OnPropertyChanged(nameof(StatusText)); } } 
        private bool _isBusy;
        public bool IsBusy { get => _isBusy; set { _isBusy = value; OnPropertyChanged(nameof(IsBusy)); } }
        private string _apiKey;
        public string ApiKey
        {
            get => _apiKey;
            set
            {
                if (_apiKey != value)
                {
                    _apiKey = value;
                    OnPropertyChanged(nameof(ApiKey));
                    SaveSettings();
                }
            }
        }
        public List<Account> SelectedAccounts { get; set; } = new List<Account>();


        // --- 命令 ---
        public ICommand AddAccountCommand { get; }
        public ICommand EditAccountCommand { get; }
        public ICommand DeleteAccountCommand { get; }
        public ICommand LoginCommand { get; }
        public ICommand BatchUpdateNicknameCommand { get; }
        public ICommand UpdateStatusCommand { get; } // [优化] 合并为一个命令
        public ICommand BulkAddCommand { get; }

        private void RebuildAccountsView()
        {
            // 1. 基于当前的 Accounts 集合创建一个新的 CollectionViewSource
            var source = new CollectionViewSource { Source = Accounts };

            // 2. 为这个新的 Source 添加排序规则
            source.SortDescriptions.Add(new SortDescription(nameof(Account.Cs2PrimeStatus), ListSortDirection.Descending));
            source.SortDescriptions.Add(new SortDescription(nameof(Account.SortPriority), ListSortDirection.Ascending));

            // 3. 将 AccountsView 属性指向这个全新的、健康的视图
            AccountsView = source.View;

            // 4. [关键] 通知 UI，AccountsView 这个属性本身已经发生了变化
            //    DataGrid 会因此重新获取它的 ItemsSource，并绑定到这个新视图上
            OnPropertyChanged(nameof(AccountsView));
        }
        public MainViewModel()
        {
            _dataService = new JsonDataService();
            _apiService = new SteamApiService();

            AddAccountCommand = new RelayCommand(AddAccount);
            EditAccountCommand = new RelayCommand(EditAccount, CanExecuteOnSingleSelect);
            DeleteAccountCommand = new RelayCommand(DeleteAccount, CanDeleteAccount);
            LoginCommand = new RelayCommand(Login, CanExecuteOnSingleSelect);
            BatchUpdateNicknameCommand = new RelayCommand(async (p) => await BatchUpdateNicknames(), CanExecuteOnMultiSelect);
            UpdateStatusCommand = new RelayCommand(async (p) => await UpdateStatus(), CanExecuteOnMultiSelect); // [优化] 绑定到新方法
            BulkAddCommand = new RelayCommand(BulkAdd);

            Accounts = new ObservableCollection<Account>(_dataService.LoadAccounts());
            RebuildAccountsView();
            InitializeCooldownChecker();
            LoadSettings();
            //var source = new CollectionViewSource { Source = Accounts };

            //source.SortDescriptions.Add(new SortDescription(nameof(Account.IsPrime), ListSortDirection.Descending));
            //source.SortDescriptions.Add(new SortDescription(nameof(Account.SortPriority), ListSortDirection.Ascending));

            //AccountsView = source.View;

            //Accounts.CollectionChanged += (s, e) =>
            //{
            //    if (AccountsView != null)
            //    {
            //        AccountsView.Refresh();
            //    }
            //};


        }


        private void InitializeCooldownChecker()
        {
            _cooldownCheckTimer = new DispatcherTimer();
            _cooldownCheckTimer.Interval = TimeSpan.FromMinutes(1); // 每分钟检查一次
            _cooldownCheckTimer.Tick += CooldownCheckTimer_Tick;
            _cooldownCheckTimer.Start();

            // 启动时立即检查一次
            CheckForExpiredCooldowns();
        }
        private void CooldownCheckTimer_Tick(object sender, EventArgs e)
        {
            CheckForExpiredCooldowns();
        }
        private void CheckForExpiredCooldowns()
        {
            // 使用 ToList() 创建副本，因为我们可能会在循环中修改集合的成员
            var cooledDownAccounts = Accounts
                .Where(acc => acc.Cs2BanStatus == "⏳冷却⏳" && acc.Cs2CooldownExpiry.HasValue && acc.Cs2CooldownExpiry.Value <= DateTime.Now)
                .ToList();

            if (cooledDownAccounts.Any())
            {
                foreach (var account in cooledDownAccounts)
                {
                    account.Cs2BanStatus = "✨正常✨"; // 这会自动清除 CooldownExpiryDate
                    StatusText = $"提示: 账号 '{account.Nickname ?? account.Username}' 的冷却时间已结束。";
                }
                // 状态已改变，保存到文件
                _dataService.SaveAccounts(Accounts);
                RebuildAccountsView();
            }
        }
        // --- CanExecute 谓词 ---
        private bool CanExecuteOnMultiSelect(object p) => SelectedAccounts.Any() && !IsBusy;
        private bool CanExecuteOnSingleSelect(object p) => SelectedAccounts.Count == 1 && !IsBusy;
        // --- 设置读写 ---
        private void LoadSettings()
        {
            var settings = _dataService.LoadSettings();
            ApiKey = settings.ApiKey;
        }
        private void SaveSettings()
        {
            var settings = _dataService.LoadSettings();
            settings.ApiKey = this.ApiKey;
            _dataService.SaveSettings(settings);
        }
        // --- CRUD 和登录操作 ---
        private void AddAccount(object parameter)
        {
            var dialog = new AddEditAccountWindow();
            if (dialog.ShowDialog() == true)
            {
                var newAccount = dialog.GetAccount();

                // [核心修正] 使用 DeferRefresh
                using (AccountsView.DeferRefresh())
                {
                    Accounts.Add(newAccount);
                }

                _dataService.SaveAccounts(Accounts);
                RebuildAccountsView();
            }
        }
        private void EditAccount(object parameter)
        {
            // 前置条件检查：虽然 CanExecute 已经保证了这一点，
            // 但在方法内部再次检查是一种良好的防御性编程习惯。
            if (SelectedAccounts.Count != 1)
            {
                // 理论上不应该执行到这里，但以防万一。
                MessageBox.Show("请确保只选择了一个账号进行编辑。", "操作无效", MessageBoxButton.OK, MessageBoxImage.Warning);
                return;
            }

            var accountToEdit = SelectedAccounts.First();

            // 创建编辑窗口，并将要编辑的账号对象（的副本）传递给它
            var dialog = new AddEditAccountWindow(accountToEdit);

            // 以模态方式显示对话框，程序会在此暂停，直到对话框关闭
            if (dialog.ShowDialog() == true)
            {
                // 如果用户点击了 "确定" (DialogResult 为 true)
                var updatedAccount = dialog.GetAccount(); // 从对话框获取修改后的数据

                // 在原始的 Accounts 集合中找到需要被更新的那个账号对象
                var originalAccount = Accounts.FirstOrDefault(a => a.Id == updatedAccount.Id);
                if (originalAccount != null)
                {
                    // 将修改后的数据逐一赋值给原始对象
                    // 因为 originalAccount 是 ObservableCollection 的成员，
                    // 并且 Account 实现了 INotifyPropertyChanged，
                    // 所以对它属性的任何修改都会立即反映在 DataGrid 上。
                    originalAccount.Username = updatedAccount.Username;
                    originalAccount.Password = updatedAccount.Password;
                    originalAccount.SteamID64 = updatedAccount.SteamID64;
                    originalAccount.Nickname = updatedAccount.Nickname;
                    originalAccount.Email = updatedAccount.Email;
                    originalAccount.EmailPassword = updatedAccount.EmailPassword;

                    originalAccount.Cs2PrimeStatus = updatedAccount.Cs2PrimeStatus;
                    originalAccount.Cs2BanStatus = updatedAccount.Cs2BanStatus;
                    originalAccount.Cs2CooldownExpiry = updatedAccount.Cs2CooldownExpiry;
                    originalAccount.IsVacBanned = updatedAccount.IsVacBanned;
                    RebuildAccountsView();
                    _dataService.SaveAccounts(Accounts);
                    StatusText = $"账号 '{originalAccount.Nickname}' 已更新。";
                    
                }
            }
        }
        private bool CanDeleteAccount(object parameter)
        {
            // parameter 是从 DataGrid 传递过来的 SelectedItems 集合
            if (parameter is IList selectedItems)
            {
                return selectedItems.Count > 0 && !IsBusy;
            }
            return false;
        }

        
        private void DeleteAccount(object parameter)
        {
            if (!(parameter is IList selectedItems) || selectedItems.Count == 0) return;
            var accountsToDelete = selectedItems.OfType<Account>().ToList();
            if (!accountsToDelete.Any()) return;

            if (MessageBox.Show($"确定要删除选中的 {accountsToDelete.Count} 个账号吗？", "确认删除", MessageBoxButton.YesNo, MessageBoxImage.Warning) == MessageBoxResult.Yes)
            {
                var idsToDelete = new HashSet<Guid>(accountsToDelete.Select(a => a.Id));
                var accountsToKeep = Accounts.Where(a => !idsToDelete.Contains(a.Id)).ToList();

                Accounts.Clear();
                foreach (var account in accountsToKeep)
                {
                    Accounts.Add(account);
                }

                _dataService.SaveAccounts(Accounts);
                StatusText = $"成功删除了 {accountsToDelete.Count} 个账号。";

                // [终极修正] 调用重建方法，而不是刷新方法
                RebuildAccountsView();
            }
        }
        private void Login(object parameter)
        {
            SteamLauncher.Login(SelectedAccounts.First());
        }
        private void BulkAdd(object parameter)
        {
            var dialog = new BulkAddWindow();
            if (dialog.ShowDialog() == true && dialog.ParsedAccounts.Any())
            {
                // 为了避免多次触发 CollectionChanged，我们可以先禁用通知
                // 但更简单的方式是直接添加
                foreach (var account in dialog.ParsedAccounts)
                {
                    Accounts.Add(account);
                }

                _dataService.SaveAccounts(Accounts);
                StatusText = $"成功添加了 {dialog.ParsedAccounts.Count} 个账号。";
                RebuildAccountsView();
            }
        }
        // --- API 相关操作 ---
        private async Task BatchUpdateNicknames()
        {
            var accountsToUpdate = SelectedAccounts.Where(a => !string.IsNullOrEmpty(a.SteamID64)).ToList();
            if (!accountsToUpdate.Any())
            {
                MessageBox.Show("选中的账号没有有效的 SteamID64。", "提示");
                return;
            }

            IsBusy = true;
            StatusText = "正在更新昵称...";
            try
            {
                var summaries = await _apiService.GetPlayerSummariesAsync(ApiKey, accountsToUpdate.Select(a => a.SteamID64));
                if (summaries.Any())
                {
                    foreach (var summary in summaries)
                    {
                        var account = accountsToUpdate.FirstOrDefault(a => a.SteamID64 == summary.SteamId);
                        if (account != null)
                        {
                            account.Nickname = summary.PersonaName;
                            StatusText = $"成功更新 {summaries.Count} 个账号的昵称。({summary.PersonaName})";
                            RebuildAccountsView();
                        }
                    }
                    _dataService.SaveAccounts(Accounts);
                    
                }
                else
                {
                    StatusText = "API 未返回有效数据。";
                }
            }
            catch (Exception ex)
            {
                StatusText = $"API 操作失败: {ex.Message}";
                MessageBox.Show(ex.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsBusy = false;
            }
        }
        private async Task UpdateStatus()
        {
            var accountsToUpdate = SelectedAccounts.Where(a => !string.IsNullOrEmpty(a.SteamID64)).ToList();
            if (!accountsToUpdate.Any())
            {
                MessageBox.Show("选中的账号没有有效的 SteamID64。", "提示");
                return;
            }

            IsBusy = true;
            StatusText = "正在更新账号状态...";
            try
            {
                var bans = await _apiService.GetPlayerBansAsync(ApiKey, accountsToUpdate.Select(a => a.SteamID64));

                // [修正] 关键逻辑：处理所有选中的账号，无论API是否返回其信息
                foreach (var account in accountsToUpdate)
                {
                    var banInfo = bans.FirstOrDefault(b => b.SteamId == account.SteamID64);
                    if (banInfo != null)
                    {
                        if (banInfo.VACBanned)
                        {
                            account.Cs2BanStatus = "VAC";
                        }
                        else if (banInfo.EconomyBan == "probation")
                        {
                            account.Cs2BanStatus = "冷却";
                        }
                        else
                        {
                            account.Cs2BanStatus = "正常";
                        }
                    }
                    else
                    {
                        account.Cs2BanStatus = "正常";
                    }
                }
                _dataService.SaveAccounts(Accounts);
                RebuildAccountsView();
                StatusText = "账号状态更新完成！";
            }
            catch (Exception ex)
            {
                StatusText = $"错误: {ex.Message}";
                MessageBox.Show(ex.Message, "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                IsBusy = false;
            }
        }
        private void SafeRefreshView()
        {
            Application.Current.Dispatcher.BeginInvoke(new Action(() =>
            {
                if (AccountsView != null)
                {
                    AccountsView.Refresh();
                }
            }), DispatcherPriority.DataBind);
        }
    }
}