/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Text.RegularExpressions;
using System.Xml;

static class StringExtensionMethods
{
	public static bool ContainsIgnoreCase(this string s, string value)
	{
		return s.IndexOf(value, StringComparison.OrdinalIgnoreCase) >= 0;
	}
}

namespace ReShade.Setup.Dialogs
{
	public partial class SelectAppDialog : Window
	{
		public SelectAppDialog()
		{
			InitializeComponent();

			ProgramList.ItemsSource = CollectionViewSource.GetDefaultView(ProgramListItems);

			UpdateThread = new Thread(() =>
			{
				var files = new List<string>();
#if !RESHADE_SETUP_USE_MUI_CACHE
				var searchPaths = new Queue<string>();

				// Add Steam install locations
				try
				{
					string steamInstallPath = Registry.LocalMachine.OpenSubKey(@"Software\Wow6432Node\Valve\Steam")?.GetValue("InstallPath") as string;
					if (!string.IsNullOrEmpty(steamInstallPath) && Directory.Exists(steamInstallPath))
					{
						searchPaths.Enqueue(Path.Combine(steamInstallPath, "steamapps", "common"));

						string steamConfig = File.ReadAllText(Path.Combine(steamInstallPath, "config", "config.vdf"));
						foreach (Match match in new Regex("\"BaseInstallFolder_[1-9]\"\\s+\"(.+)\"").Matches(steamConfig))
						{
							searchPaths.Enqueue(Path.Combine(match.Groups[1].Value.Replace("\\\\", "\\"), "steamapps", "common"));
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
								searchPaths.Enqueue(searchPath);
							}
						}
					}
				}
				catch { }

				// Add Epic Games Launcher install location
				{
					string epicGamesInstallPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Epic Games");
					if (Directory.Exists(epicGamesInstallPath))
					{
						searchPaths.Enqueue(epicGamesInstallPath);
					}
				}
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
				var items = new KeyValuePair<string, FileVersionInfo>[SPLIT_SIZE];

#if !RESHADE_SETUP_USE_MUI_CACHE
				while (searchPaths.Count != 0)
				{
					string searchPath = searchPaths.Dequeue();

					try
					{
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

						for (int i = 0; i < Math.Min(SPLIT_SIZE, files.Count - offset); ++i)
						{
							string path = files[offset + i];

							if (Path.GetExtension(path) != ".exe" || !File.Exists(path))
							{
								continue;
							}

							// Exclude common installer, support and launcher executables
							if (path.ContainsIgnoreCase("redis") ||
								path.ContainsIgnoreCase("unins") ||
								path.ContainsIgnoreCase("setup") ||
								path.ContainsIgnoreCase("patch") ||
								path.ContainsIgnoreCase("update") ||
								path.ContainsIgnoreCase("install") ||
								path.ContainsIgnoreCase("report") ||
								path.ContainsIgnoreCase("support") ||
								path.ContainsIgnoreCase("register") ||
								path.ContainsIgnoreCase("activation") ||
								path.ContainsIgnoreCase("diagnostics") ||
								path.ContainsIgnoreCase("tool") ||
								path.ContainsIgnoreCase("crash") ||
								path.ContainsIgnoreCase("config") ||
								path.ContainsIgnoreCase("launch") ||
								path.ContainsIgnoreCase("plugin") ||
								path.ContainsIgnoreCase("benchmark") ||
								path.ContainsIgnoreCase("steamvr") ||
								path.ContainsIgnoreCase("cefprocess") ||
								path.Contains("svc") ||
								// Ignore certain folders that are unlikely to contain useful executables
								path.ContainsIgnoreCase("docs") ||
								path.ContainsIgnoreCase("cache") ||
								path.ContainsIgnoreCase("support") ||
								path.Contains("Data") || // AppData, ProgramData, _Data
								path.Contains("_CommonRedist") ||
								path.Contains("__Installer") ||
								path.Contains("\\$") ||
								path.Contains("\\.") ||
								path.Contains("\\Windows") ||
								path.ContainsIgnoreCase("steamvr"))
							{
								continue;
							}

							items[i] = new KeyValuePair<string, FileVersionInfo>(path, FileVersionInfo.GetVersionInfo(path));
						}

						Dispatcher.Invoke(new Action<KeyValuePair<string, FileVersionInfo>[]>(arg =>
						{
							foreach (var item in arg)
							{
								if (item.Key == null || ProgramListItems.Any(x => x.Path == item.Key))
								{
									continue;
								}

								ProgramListItems.Add(new ProgramItem(item.Key, item.Value));
							}
						}), DispatcherPriority.Background, (object)items);

						// Give back control to the OS
						Thread.Sleep(5);
					}

#if !RESHADE_SETUP_USE_MUI_CACHE
					// Continue searching in sub-directories
					foreach (var path in Directory.GetDirectories(searchPath))
					{
						searchPaths.Enqueue(path);
					}
				}
#endif

				// Hide progress bar after search has finished
				Dispatcher.BeginInvoke(new Action(() =>
				{
					ProgramListProgress.Visibility = Visibility.Collapsed;
				}));
			});

			UpdateThread.Start();
		}

		class ProgramItem
		{
			public ProgramItem(string path, FileVersionInfo info)
			{
				Path = path;
				Name = info.FileDescription;
				if (Name is null || Name.Trim().Length == 0)
				{
					Name = System.IO.Path.GetFileNameWithoutExtension(path);
				}

				Name += " (" + System.IO.Path.GetFileName(path) + ")";

				using (var ico = System.Drawing.Icon.ExtractAssociatedIcon(path))
				{
					Icon = Imaging.CreateBitmapSourceFromHIcon(ico.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
				}

				try
				{
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

		Thread UpdateThread = null;
		AutoResetEvent SuspendUpdateThreadEvent = new AutoResetEvent(false);
		bool SuspendUpdateThread = false;
		public string FileName { get => PathBox.Text; private set => PathBox.Text = value; }
		ObservableCollection<ProgramItem> ProgramListItems = new ObservableCollection<ProgramItem>();

		void OnBrowse(object sender, RoutedEventArgs e)
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
				FileName = dlg.FileName;
				DialogResult = true;
			}
			else
			{
				SuspendUpdateThread = false;
				SuspendUpdateThreadEvent.Set();
			}
		}

		void OnCancel(object sender, RoutedEventArgs e)
		{
			UpdateThread.Abort();
			DialogResult = false;
		}

		void OnConfirm(object sender, RoutedEventArgs e)
		{
			// Only close dialog if an actual item was selected in the list
			if (!string.IsNullOrEmpty(FileName) && Path.GetExtension(FileName) == ".exe" && File.Exists(FileName))
			{
				UpdateThread.Abort();
				DialogResult = true;
			}
		}

		void OnConfirmSelection(object sender, MouseButtonEventArgs e)
		{
			if (e.ChangedButton == MouseButton.Left && e.ButtonState == MouseButtonState.Pressed)
			{
				OnConfirm(sender, null);
			}
		}

		void OnPathChanged(object sender, System.Windows.Controls.TextChangedEventArgs e)
		{
			OnSortByChanged(sender, null);

			if (PathBox.IsFocused)
			{
				ProgramList.UnselectAll();
			}
		}
		void OnPathGotFocus(object sender, RoutedEventArgs e)
		{
			Dispatcher.BeginInvoke(new Action(() => PathBox.SelectAll()));
		}

		void OnSortByChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
		{
			var view = CollectionViewSource.GetDefaultView(ProgramListItems);

			if (PathBox != null && PathBox.Text != "Search" && !Path.IsPathRooted(PathBox.Text))
			{
				view.Filter = item => ((ProgramItem)item).Path.ContainsIgnoreCase(PathBox.Text) || ((ProgramItem)item).Name.ContainsIgnoreCase(PathBox.Text);
			}
			else
			{
				view.Filter = null;
			}

			view.SortDescriptions.Clear();
			switch (SortBy.SelectedIndex)
			{
				case 0:
					view.SortDescriptions.Add(new SortDescription(nameof(ProgramItem.LastAccess), ListSortDirection.Descending));
					break;
				case 1:
					view.SortDescriptions.Add(new SortDescription(nameof(ProgramItem.Name), ListSortDirection.Ascending));
					break;
				case 2:
					view.SortDescriptions.Add(new SortDescription(nameof(ProgramItem.Name), ListSortDirection.Descending));
					break;
			}
		}
		void OnSelectionChanged(object sender, System.Windows.Controls.SelectionChangedEventArgs e)
		{
			if (ProgramList.SelectedItem is ProgramItem item)
			{
				FileName = item.Path;
			}
		}
	}
}
