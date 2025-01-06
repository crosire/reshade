/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Xml;
using Microsoft.Win32;

static class StringExtensionMethods
{
	public static bool ContainsIgnoreCase(this string s, string value)
	{
		return s.IndexOf(value, StringComparison.OrdinalIgnoreCase) >= 0;
	}
}

static class HashSetExtensionMethods
{
	public static T Dequeue<T>(this HashSet<T> items)
	{
		if (items.Count == 0)
		{
			throw new InvalidOperationException();
		}

		var item = items.First();
		items.Remove(item);
		return item;
	}
}

namespace ReShade.Setup.Pages
{
	class App
	{
		public App(string path, FileVersionInfo info)
		{
			Path = path;
			Name = info.FileDescription;
			if (Name is null || Name.Trim().Length == 0)
			{
				Name = System.IO.Path.GetFileName(path);

				int length = Name.LastIndexOf('.');
				if (length > 1)
				{
					Name = Name.Substring(0, length);
				}

				if (char.IsLower(Name.First()))
				{
					Name = CultureInfo.CurrentCulture.TextInfo.ToTitleCase(Name);
				}
			}

			Name += " (" + System.IO.Path.GetFileName(path) + ")";

			try
			{
				using (var ico = System.Drawing.Icon.ExtractAssociatedIcon(path))
				{
					Icon = Imaging.CreateBitmapSourceFromHIcon(ico.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
				}

				LastAccess = File.GetLastAccessTime(path).ToString("s");
			}
			catch
			{
				LastAccess = string.Empty;
			}
		}

		public string Name { get; }
		public string Path { get; }
		public ImageSource Icon { get; }
		public string LastAccess { get; }
	}

	public partial class SelectAppPage : Page
	{
		public SelectAppPage()
		{
			InitializeComponent();

			ItemsListBox.ItemsSource = CollectionViewSource.GetDefaultView(Items);

			UpdateThread = new Thread(() =>
			{
				var files = new List<string>();
#if !RESHADE_SETUP_USE_MUI_CACHE
				var searchPaths = new HashSet<string>();

				// Add Steam install locations
				try
				{
					string steamInstallPath = Registry.LocalMachine.OpenSubKey(@"Software\Wow6432Node\Valve\Steam")?.GetValue("InstallPath") as string;
					if (!string.IsNullOrEmpty(steamInstallPath) && Directory.Exists(steamInstallPath))
					{
						searchPaths.Add(Path.Combine(steamInstallPath, "steamapps", "common"));

						string steamConfig = File.ReadAllText(Path.Combine(steamInstallPath, "config", "libraryfolders.vdf"));
						foreach (Match match in new Regex("\"path\"\\s+\"(.+)\"").Matches(steamConfig))
						{
							searchPaths.Add(Path.Combine(match.Groups[1].Value.Replace("\\\\", "\\"), "steamapps", "common"));
						}
					}
				}
				catch { }

				// Add Origin install locations
				try
				{
					string originConfigPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Origin", "local.xml");
					if (File.Exists(originConfigPath))
					{
						var originConfig = new XmlDocument();
						originConfig.Load(originConfigPath);

						foreach (string searchPath in originConfig["Settings"].ChildNodes.Cast<XmlNode>()
							.Where(x => x.Attributes["key"].Value == "DownloadInPlaceDir")
							.Select(x => x.Attributes["value"].Value))
						{
							// Avoid adding short paths to the search paths so not to scan the entire drive
							if (searchPath.Length > 25)
							{
								searchPaths.Add(searchPath);
							}
						}
					}
				}
				catch { }

				// Add EA Desktop install locations
				try
				{
					string eaDesktopConfigPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Electronic Arts", "EA Desktop");
					if (Directory.Exists(eaDesktopConfigPath))
					{
						foreach (string userConfigPath in Directory.EnumerateFiles(eaDesktopConfigPath, "user_*.ini"))
						{
							var userConfig = new Utilities.IniFile(userConfigPath);
							var searchPath = userConfig.GetString(string.Empty, "user.downloadinplacedir");

							if (!string.IsNullOrEmpty(searchPath) && Directory.Exists(searchPath))
							{
								if (searchPath.Last() == Path.DirectorySeparatorChar)
								{
									searchPath = searchPath.Remove(searchPath.Length - 1);
								}

								searchPaths.Add(searchPath);
							}
						}
					}
				}
				catch { }

				// Add Epic Games Launcher install location
				try
				{
					string epicGamesConfigPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EpicGamesLauncher", "Saved", "Config", "Windows", "GameUserSettings.ini");
					if (File.Exists(epicGamesConfigPath))
					{
						var epicGamesConfig = new Utilities.IniFile(epicGamesConfigPath);
						var searchPath = epicGamesConfig.GetString("Launcher", "DefaultAppInstallLocation");

						if (!string.IsNullOrEmpty(searchPath) && Directory.Exists(searchPath))
						{
							searchPaths.Add(searchPath);
						}
					}
				}
				catch { }

				// Add GOG Galaxy install locations
				try
				{
					var gogGamesKey = Registry.LocalMachine.OpenSubKey(@"Software\Wow6432Node\GOG.com\Games");
					if (gogGamesKey != null)
					{
						foreach (var gogGame in gogGamesKey.GetSubKeyNames())
						{
							string gameDir = gogGamesKey.OpenSubKey(gogGame)?.GetValue("path") as string;
							if (!string.IsNullOrEmpty(gameDir) && Directory.Exists(gameDir))
							{
								searchPaths.Add(gameDir);
							}
						}
					}
				}
				catch { }
#else
				foreach (var name in new string[] {
					"Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\Shell\\MuiCache",
					"Software\\Microsoft\\Windows\\ShellNoRoam\\MUICache",
					"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Persisted",
					"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Compatibility Assistant\\Store" })
				{
					try
					{
						string[] values = Registry.CurrentUser.OpenSubKey(name)?.GetValueNames();
						if (values != null)
						{
							files.AddRange(values);
						}
					}
					catch
					{
						// Ignore permission errors
						continue;
					}
				}
#endif

				const int SPLIT_SIZE = 50;

#if !RESHADE_SETUP_USE_MUI_CACHE
				while (searchPaths.Count != 0)
				{
					string searchPath = searchPaths.Dequeue();

					if (searchPath.Last() != Path.DirectorySeparatorChar)
					{
						searchPath += Path.DirectorySeparatorChar;
					}

					// Ignore certain folders that are unlikely to contain useful executables
					if (searchPath.ContainsIgnoreCase("docs") ||
						searchPath.ContainsIgnoreCase("cache") ||
						searchPath.ContainsIgnoreCase("support") ||
						searchPath.Contains("Data") || // AppData, ProgramData, _Data
						searchPath.Contains("_CommonRedist") ||
						searchPath.Contains("__Installer") ||
						searchPath.Contains("\\$") ||
						searchPath.Contains("\\.") ||
						searchPath.Contains("\\Windows") ||
						searchPath.Contains("\\System Volume Information") ||
						searchPath.ContainsIgnoreCase("java") ||
						searchPath.ContainsIgnoreCase("steamvr") ||
						searchPath.Contains("\\Portal\\bin") ||
						searchPath.Contains("DotNet"))
					{
						continue;
					}

					try
					{
						if (searchPath != Path.GetPathRoot(searchPath))
						{
							// Ignore hidden directories and symlinks
							var attributes = File.GetAttributes(searchPath);
							if (attributes.HasFlag(FileAttributes.Hidden) || attributes.HasFlag(FileAttributes.ReparsePoint))
							{
								continue;
							}
						}

						files = Directory.GetFiles(searchPath, "*.exe", SearchOption.TopDirectoryOnly).ToList();
					}
					catch
					{
						// Skip permission errors
						continue;
					}
#endif

					for (int offset = 0; offset < files.Count; offset += SPLIT_SIZE)
					{
						if (SuspendUpdateThread)
						{
							SuspendUpdateThreadEvent.WaitOne();
						}

						var items = new KeyValuePair<string, FileVersionInfo>[SPLIT_SIZE];

						for (int i = 0; i < Math.Min(SPLIT_SIZE, files.Count - offset); ++i)
						{
							string path = files[offset + i];

							if (!Path.GetExtension(path).Equals(".exe", StringComparison.OrdinalIgnoreCase) || !File.Exists(path))
							{
								continue;
							}

							// Exclude common installer, support and launcher executables
							if (path.ContainsIgnoreCase("activation") ||
								path.ContainsIgnoreCase("apputil") ||
								path.ContainsIgnoreCase("benchmark") ||
								path.ContainsIgnoreCase("cefprocess") ||
								path.ContainsIgnoreCase("compile") ||
								path.ContainsIgnoreCase("compress") ||
								path.ContainsIgnoreCase("config") ||
								path.ContainsIgnoreCase("console") ||
								path.ContainsIgnoreCase("convert") ||
								path.ContainsIgnoreCase("crash") ||
								path.ContainsIgnoreCase("dotnetfx") ||
								path.ContainsIgnoreCase("diagnostics") ||
								path.ContainsIgnoreCase("download") ||
								path.ContainsIgnoreCase("error") ||
								path.ContainsIgnoreCase("handler") ||
								path.ContainsIgnoreCase("helper") ||
								path.ContainsIgnoreCase("inject") ||
								path.ContainsIgnoreCase("install") ||
								path.ContainsIgnoreCase("launch") ||
								path.ContainsIgnoreCase("openvr") ||
								path.ContainsIgnoreCase("overlay") ||
								path.ContainsIgnoreCase("patch") ||
								path.ContainsIgnoreCase("plugin") ||
								path.ContainsIgnoreCase("redis") ||
								path.ContainsIgnoreCase("register") ||
								path.ContainsIgnoreCase("report") ||
								path.ContainsIgnoreCase("server") ||
								path.ContainsIgnoreCase("service") ||
								path.ContainsIgnoreCase("setup") ||
								path.ContainsIgnoreCase("steamvr") ||
								path.ContainsIgnoreCase("subprocess") ||
								path.ContainsIgnoreCase("support") ||
								path.ContainsIgnoreCase("tool") ||
								path.ContainsIgnoreCase("unins") ||
								path.ContainsIgnoreCase("update") ||
								path.ContainsIgnoreCase("util") ||
								path.ContainsIgnoreCase("validate") ||
								path.ContainsIgnoreCase("wallpaper") ||
								path.ContainsIgnoreCase("webengine") ||
								path.ContainsIgnoreCase("webview") ||
								path.Contains("7za") ||
								path.Contains("createdump") ||
								path.Contains("fossilize") ||
								path.Contains("Rpt") || // CrashRpt, SndRpt
								path.Contains("svc") ||
								path.Contains("SystemSoftware") ||
								path.Contains("TagesClient"))
							{
								continue;
							}

							items[i] = new KeyValuePair<string, FileVersionInfo>(path, FileVersionInfo.GetVersionInfo(path));
						}

						Dispatcher.Invoke(new Action<KeyValuePair<string, FileVersionInfo>[]>(arg =>
						{
							foreach (var item in arg)
							{
								if (string.IsNullOrEmpty(item.Key) || Items.Any(x => x.Path == item.Key))
								{
									continue;
								}

								Items.Add(new App(item.Key, item.Value));
							}
						}), DispatcherPriority.Background, items);

						// Give back control to the OS
						Thread.Sleep(5);
					}

#if !RESHADE_SETUP_USE_MUI_CACHE
					try
					{
						// Continue searching in sub-directories
						foreach (var path in Directory.GetDirectories(searchPath))
						{
							searchPaths.Add(path);
						}
					}
					catch
					{
						// Skip permission errors
						continue;
					}
				}
#endif
			});

			UpdateThread.IsBackground = true;
			UpdateThread.Start();
		}

		readonly Thread UpdateThread = null;
		readonly AutoResetEvent SuspendUpdateThreadEvent = new AutoResetEvent(false);
		bool SuspendUpdateThread = false;
		bool IgnorePathBoxChanged = false;
		readonly ObservableCollection<App> Items = new ObservableCollection<App>();

		public string FileName
		{
			get => PathBox.Text;
			set => PathBox.Text = value;
		}

		public void Cancel()
		{
			UpdateThread.Abort();
		}

		private void OnBrowseClick(object sender, RoutedEventArgs e)
		{
			SuspendUpdateThread = true;

			var dlg = new OpenFileDialog
			{
				Filter = "Applications|*.exe",
				DefaultExt = ".exe",
				Multiselect = false,
				ValidateNames = true,
				CheckFileExists = true
			};

			if (dlg.ShowDialog() == true)
			{
				UpdateThread.Abort();

				PathBox.Focus();
				FileName = dlg.FileName;
			}
			else
			{
				SuspendUpdateThread = false;
				SuspendUpdateThreadEvent.Set();
			}
		}

		private void OnPathGotFocus(object sender, RoutedEventArgs e)
		{
			Dispatcher.BeginInvoke(new Action(PathBox.SelectAll));
		}

		private void OnPathTextChanged(object sender, TextChangedEventArgs e)
		{
			if (IgnorePathBoxChanged)
			{
				return;
			}

			var invalidChars = Path.GetInvalidPathChars();
			for (int i = 0; i < PathBox.Text.Length;)
			{
				if (invalidChars.Contains(PathBox.Text[i]))
				{
					PathBox.Text = PathBox.Text.Remove(i, 1);
				}
				else
				{
					i++;
				}
			}

			OnSortByChanged(sender, null);

			if (PathBox.IsFocused)
			{
				ItemsListBox.UnselectAll();
			}
		}

		private void OnSortByChanged(object sender, SelectionChangedEventArgs e)
		{
			var view = CollectionViewSource.GetDefaultView(Items);

			if (PathBox != null && PathBox.Text != "Search" && !Path.IsPathRooted(PathBox.Text))
			{
				view.Filter = item => ((App)item).Path.ContainsIgnoreCase(PathBox.Text) || ((App)item).Name.ContainsIgnoreCase(PathBox.Text);
			}
			else
			{
				view.Filter = null;
			}

			view.SortDescriptions.Clear();
			switch (SortBy.SelectedIndex)
			{
				case 0:
					view.SortDescriptions.Add(new SortDescription(nameof(App.LastAccess), ListSortDirection.Descending));
					break;
				case 1:
					view.SortDescriptions.Add(new SortDescription(nameof(App.Name), ListSortDirection.Ascending));
					break;
				case 2:
					view.SortDescriptions.Add(new SortDescription(nameof(App.Name), ListSortDirection.Descending));
					break;
			}
		}

		private void OnSelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			if (ItemsListBox.SelectedItem is App item)
			{
				IgnorePathBoxChanged = true;
				FileName = item.Path;
				IgnorePathBoxChanged = false;

				ItemsListBox.ScrollIntoView(item);
			}
		}
	}
}
