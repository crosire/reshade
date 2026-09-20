using System;
using System.Windows.Data;
using System.Runtime.InteropServices;

namespace ReShade.Setup
{
	public partial class App
	{
		[DllImport("kernel32.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		static extern bool SetDefaultDllDirectories(uint directoryFlags);

		public App()
		{
			if (Type.GetType("Mono.Runtime") == null)
			{
				// The 'DefaultDllImportSearchPaths' assembly attribute does not cover DLLs loaded by the WPF runtime, so force it to load from the system directory via native call
				SetDefaultDllDirectories(0x00000800 /* LOAD_LIBRARY_SEARCH_SYSTEM32 */);
			}
		}
	}

	public class MultiplyValueConverter : IValueConverter
	{
		public double Ratio { get; set; }

		public object Convert(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			return (double)value * Ratio;
		}

		public object ConvertBack(object value, Type targetType, object parameter, System.Globalization.CultureInfo culture)
		{
			throw new NotImplementedException();
		}
	}
}
