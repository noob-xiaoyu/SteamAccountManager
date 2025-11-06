// Views/AddEditAccountWindow.xaml.cs
using SteamAccountManager.Models;
using SteamAccountManager.ViewModels;
using System;
using System.Windows;

namespace SteamAccountManager.Views
{
    /// <summary>
    /// AddEditAccountWindow.xaml 的交互逻辑。
    /// 这个窗口的后台代码（Code-behind）遵循 MVVM 设计模式，职责非常单一：
    /// 1. 创建并关联对应的 ViewModel。
    /// 2. 提供一个公共方法，让外部调用者可以获取最终的数据结果。
    /// 它不包含任何业务逻辑（如数据验证、计算等），所有逻辑都在 ViewModel 中处理。
    /// </summary>
    public partial class AddEditAccountWindow : Window
    {
        /// <summary>
        /// 构造函数，用于初始化窗口。
        /// </summary>
        /// <param name="accountToEdit">
        /// 一个可选的 Account 对象。
        /// 如果为 null，表示是“添加新账号”模式。
        /// 如果不为 null，表示是“编辑现有账号”模式。
        /// </param>
        public AddEditAccountWindow(Account accountToEdit = null)
        {
            // InitializeComponent() 是 WPF 框架的必要方法，用于加载和渲染 XAML 中定义的 UI 元素。
            InitializeComponent();

            // 创建与此视图对应的 ViewModel 实例。
            // 所有的业务逻辑、数据状态都封装在 ViewModel 中。
            var viewModel = new AddEditAccountViewModel(accountToEdit);

            // 将 ViewModel 实例设置为当前窗口的数据上下文（DataContext）。
            // 这是实现数据绑定的关键步骤。从此，XAML 中的 {Binding ...} 表达式
            // 就会自动去 viewModel 对象中寻找对应的属性。
            this.DataContext = viewModel;
        }

        /// <summary>
        /// 一个公共方法，供父窗口（如 MainWindow）调用，以获取用户在窗口中输入或修改后的账号数据。
        /// </summary>
        /// <returns>返回一个 Account 对象，包含了最终确认的数据。如果 ViewModel 不存在或类型不正确，则返回 null。</returns>
        public Account GetAccount()
        {
            // 尝试将当前窗口的 DataContext 转换为我们期望的 AddEditAccountViewModel 类型。
            // 使用 'as' 关键字进行类型转换，如果转换失败，它会返回 null，而不会抛出异常，这比强制转换更安全。
            if (this.DataContext is AddEditAccountViewModel viewModel)
            {
                // 如果类型转换成功，说明 DataContext 是正确的。
                // 我们直接返回 ViewModel 中持有的 Account 对象。
                // 这个 Account 对象包含了所有通过数据绑定从 UI 更新的数据。
                return viewModel.Account;
            }
            else
            {
                // 如果 DataContext 不是我们期望的类型（理论上不应该发生，但作为防御性编程，最好有这个分支），
                // 记录一个错误或直接返回 null。
                // 这可以防止程序在意外情况下崩溃。
                Console.WriteLine("错误: AddEditAccountWindow 的 DataContext 类型不正确。");
                return null;
            }
        }
    }
}