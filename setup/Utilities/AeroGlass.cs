using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace ReShade.Setup.Utilities
{
	public static class AeroGlass
	{
		[StructLayout(LayoutKind.Sequential)]
		struct MARGINS
		{
			public int cxLeftWidth;
			public int cxRightWidth;
			public int cyTopHeight;
			public int cyBottomHeight;
		}

		[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
		struct SHSTOCKICONINFO
		{
			public uint cbSize;
			public IntPtr hIcon;
			public uint iSysIconIndex;
			public uint iIcon;
			[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260 /*MAX_PATH*/)]
			public string szPath;
		}

		[DllImport("dwmapi.dll")]
		static extern int DwmIsCompositionEnabled(out bool enabled);
		[DllImport("dwmapi.dll")]
		static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref MARGINS pMarInset);

		[DllImport("user32.dll")]
		static extern int GetWindowLong(IntPtr hwnd, int index);
		[DllImport("user32.dll")]
		static extern int SetWindowLong(IntPtr hwnd, int index, int newStyle);

		[DllImport("user32.dll")]
		static extern bool SetWindowPos(IntPtr hwnd, IntPtr hwndInsertAfter, int x, int y, int width, int height, uint flags);

		[DllImport("user32.dll")]
		static extern IntPtr SendMessage(IntPtr hwnd, uint msg, ulong wParam, ulong lParam);

		[DllImport("user32.dll")]
		static extern int DestroyIcon(IntPtr hIcon);

		[DllImport("shell32.dll", SetLastError = false)]
		static extern int SHGetStockIconInfo(uint siid, uint uFlags, ref SHSTOCKICONINFO psii);

		public static bool IsEnabled
		{
			get
			{
				// Only enable Aero Glass on Windows 7
				if (Environment.OSVersion.Version.Major != 6 || Environment.OSVersion.Version.Minor != 1)
				{
					return false;
				}

				DwmIsCompositionEnabled(out bool enabled);
				return enabled;
			}
		}

		public static void HideIcon(Window window)
		{
			IntPtr hwnd = new WindowInteropHelper(window).Handle;

			const int WM_SETICON = 0x0080;

			SendMessage(hwnd, WM_SETICON, 1, 0);
			SendMessage(hwnd, WM_SETICON, 0, 0);

			const int GWL_EXSTYLE = -20;
			const int WS_EX_DLGMODALFRAME = 0x0001;

			SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_DLGMODALFRAME);

			const int SWP_NOSIZE = 0x0001;
			const int SWP_NOMOVE = 0x0002;
			const int SWP_NOZORDER = 0x0004;
			const int SWP_FRAMECHANGED = 0x0020;

			SetWindowPos(hwnd, IntPtr.Zero, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}
		public static void HideSystemMenu(Window window, bool hidden = true)
		{
			IntPtr hwnd = new WindowInteropHelper(window).Handle;

			const int GWL_STYLE = -16;
			const int WS_SYSMENU = 0x80000;

			if (hidden)
			{
				SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) & ~WS_SYSMENU);
			}
			else
			{
				SetWindowLong(hwnd, GWL_STYLE, GetWindowLong(hwnd, GWL_STYLE) | WS_SYSMENU);
			}
		}

		public static bool ExtendFrame(Window window)
		{
			return ExtendFrame(window, new Thickness(-1, -1, -1, -1));
		}
		public static bool ExtendFrame(Window window, Thickness margin)
		{
			if (!IsEnabled)
			{
				return false;
			}

			IntPtr hwnd = new WindowInteropHelper(window).Handle;

			if (hwnd == IntPtr.Zero)
			{
				throw new InvalidOperationException("The window must be shown before extending glass effect.");
			}

			// Adapted from http://blogs.msdn.com/b/adam_nathan/archive/2006/05/04/589686.aspx
			window.Background = Brushes.Transparent;
			HwndSource.FromHwnd(hwnd).CompositionTarget.BackgroundColor = Colors.Transparent;

			var margins = new MARGINS
			{
				cxLeftWidth = (int)margin.Left,
				cxRightWidth = (int)margin.Right,
				cyTopHeight = (int)margin.Top,
				cyBottomHeight = (int)margin.Bottom
			};

			DwmExtendFrameIntoClientArea(hwnd, ref margins);

			return true;
		}

		public static BitmapSource UacShieldIcon
		{
			get
			{
				var sii = new SHSTOCKICONINFO();
				sii.cbSize = (uint)Marshal.SizeOf(typeof(SHSTOCKICONINFO));

				Marshal.ThrowExceptionForHR(SHGetStockIconInfo(77 /*SIID_SHIELD*/,
					0x000000100 /*SHGSI.SHGSI_ICON*/ | 0x000000001 /*SHGSI.SHGSI_SMALLICON*/,
					ref sii));

				var shieldSource = Imaging.CreateBitmapSourceFromHIcon(
					sii.hIcon,
					Int32Rect.Empty,
					BitmapSizeOptions.FromEmptyOptions());

				DestroyIcon(sii.hIcon);

				return shieldSource;
			}
		}
	}
}
