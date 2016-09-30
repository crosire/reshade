using System.Text;
using System.Runtime.InteropServices;

public static class IniFile
{
	[DllImport("kernel32", CharSet = CharSet.Unicode)]
	private static extern int GetPrivateProfileString(string section, string key, string defaultValue, StringBuilder value, int size, string filePath);
	[DllImport("kernel32", CharSet = CharSet.Unicode, SetLastError = true)]
	[return: MarshalAs(UnmanagedType.Bool)]
	private static extern bool WritePrivateProfileString(string section, string key, string value, string filePath);

	public static string ReadValue(string file, string section, string key, string defaultValue = "")
	{
		var value = new StringBuilder(512);
		GetPrivateProfileString(section, key, defaultValue, value, value.Capacity, file);
		return value.ToString();
	}
	public static bool WriteValue(string file, string section, string key, string value)
	{
		return WritePrivateProfileString(section, key, value, file);
	}
}
