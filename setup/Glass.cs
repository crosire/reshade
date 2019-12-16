using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using System.Windows.Media;

public static class Glass
{
	[StructLayout(LayoutKind.Sequential)]
	private struct MARGINS
	{
		public int cxLeftWidth;
		public int cxRightWidth;
		public int cyTopHeight;
		public int cyBottomHeight;
	}

	[DllImport("dwmapi.dll")]
	private static extern int DwmIsCompositionEnabled(out bool enabled);
	[DllImport("dwmapi.dll")]
	private static extern int DwmExtendFrameIntoClientArea(IntPtr hwnd, ref MARGINS pMarInset);

	[DllImport("user32.dll")]
	private static extern int GetWindowLong(IntPtr hwnd, int index);
	[DllImport("user32.dll")]
	private static extern int SetWindowLong(IntPtr hwnd, int index, int newStyle);
	[DllImport("user32.dll")]
	private static extern bool SetWindowPos(IntPtr hwnd, IntPtr hwndInsertAfter, int x, int y, int width, int height, uint flags);

	[DllImport("user32.dll")]
	private static extern IntPtr SendMessage(IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam);

	public static bool IsEnabled
	{
		get
		{
			DwmIsCompositionEnabled(out bool enabled);
			return enabled;
		}
	}

	public static void HideIcon(Window window)
	{
		IntPtr hwnd = new WindowInteropHelper(window).Handle;

		const int WM_SETICON = 0x0080;

		SendMessage(hwnd, WM_SETICON, new IntPtr(1), IntPtr.Zero);
		SendMessage(hwnd, WM_SETICON, IntPtr.Zero, IntPtr.Zero);

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

		var margins = new MARGINS {
			cxLeftWidth = (int)margin.Left,
			cxRightWidth = (int)margin.Right,
			cyTopHeight = (int)margin.Top,
			cyBottomHeight = (int)margin.Bottom
		};

		DwmExtendFrameIntoClientArea(hwnd, ref margins);

		return true;
	}
}
