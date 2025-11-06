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
            var source = new CollectionViewSource { Source = Accounts };
            source.SortDescriptions.Add(new SortDescription(nameof(Account.IsPrime), ListSortDirection.Descending));
            source.SortDescriptions.Add(new SortDescription(nameof(Account.SortPriority), ListSortDirection.Ascending));
            AccountsView = source.View;
            OnPropertyChanged(nameof(AccountsView)); // 通知 UI 视图已更新
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

            var source = new CollectionViewSource { Source = Accounts };

            source.SortDescriptions.Add(new SortDescription(nameof(Account.IsPrime), ListSortDirection.Descending));
            source.SortDescriptions.Add(new SortDescription(nameof(Account.SortPriority), ListSortDirection.Ascending));

            AccountsView = source.View;

            Accounts.CollectionChanged += (s, e) =>
            {
                if (AccountsView != null)
                {
                    AccountsView.Refresh();
                }
            };

            InitializeCooldownChecker();
            LoadSettings();
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
                .Where(acc => acc.Status == "冷却" && acc.CooldownExpiryDate.HasValue && acc.CooldownExpiryDate.Value <= DateTime.Now)
                .ToList();

            if (cooledDownAccounts.Any())
            {
                foreach (var account in cooledDownAccounts)
                {
                    account.Status = "正常"; // 这会自动清除 CooldownExpiryDate
                    StatusText = $"提示: 账号 '{account.Nickname ?? account.Username}' 的冷却时间已结束。";
                }
                // 状态已改变，保存到文件
                _dataService.SaveAccounts(Accounts);
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
                    originalAccount.Status = updatedAccount.Status;
                    originalAccount.CooldownExpiryDate = updatedAccount.CooldownExpiryDate;

                    // 将更新后的数据保存到 accounts.json 文件
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
                // 1. 筛选出要保留的账号
                var idsToDelete = new HashSet<Guid>(accountsToDelete.Select(a => a.Id));
                var accountsToKeep = Accounts.Where(a => !idsToDelete.Contains(a.Id)).ToList();

                // 2. 清空原始集合，这会触发一次 Reset 通知
                Accounts.Clear();

                // 3. 将保留的账号重新添加回去
                foreach (var account in accountsToKeep)
                {
                    Accounts.Add(account);
                }

                // 4. 保存和更新状态
                _dataService.SaveAccounts(Accounts);
                StatusText = $"成功删除了 {accountsToDelete.Count} 个账号。";

                // 5. 虽然 CollectionView 会自动更新，但为了确保排序等完全正确，
                //    在批量操作后手动刷新一次是好习惯。
                //    我们使用之前最安全的 Dispatcher 方式。
                SafeRefreshView();
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
                SafeRefreshView();
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
                        // API 返回了该账号的信息，说明它至少有某种标记
                        account.IsVacBanned = banInfo.VACBanned;
                        account.NumberOfGameBans = banInfo.NumberOfGameBans;
                        account.DaysSinceLastBan = banInfo.DaysSinceLastBan;

                        // [实现最终需求] 正常/冷却/VAC 的判断逻辑
                        if (banInfo.VACBanned || banInfo.NumberOfGameBans > 0)
                        {
                            account.Status = "VAC";
                        }
                        else if (banInfo.EconomyBan == "probation")
                        {
                            account.Status = "冷却";
                        }
                        else
                        {
                            account.Status = "正常";
                        }
                    }
                    else
                    {
                        // API 没有返回该账号的信息，说明它是完全干净的
                        account.Status = "正常";
                        account.IsVacBanned = false;
                        account.NumberOfGameBans = 0;
                    }
                }
                _dataService.SaveAccounts(Accounts);
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