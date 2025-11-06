// Helpers/InverseBooleanConverter.cs
using System;
using System.Globalization;
using System.Windows.Data;

// 确保命名空间是这个，与你的文件夹结构和 XAML 引用完全对应
namespace SteamAccountManager.Helpers
{
    [ValueConversion(typeof(bool), typeof(bool))]
    public class InverseBooleanConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return !boolValue;
            }
            return value;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}