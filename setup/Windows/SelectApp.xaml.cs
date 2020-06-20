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
using System.Text.RegularExpressions;
using System.Threading;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using System.Xml;

namespace ReShade.Setup.Dialogs
{
	public partial class SelectAppDialog : Window
	{
		public SelectAppDialog()
		{
			InitializeComponent();

			// Sort items in list by name
			var view = CollectionViewSource.GetDefaultView(ProgramListItems);
			view.SortDescriptions.Add(new SortDescription(nameof(ProgramItem.Name), ListSortDirection.Ascending));
			ProgramList.ItemsSource = view;

			UpdateThread = new Thread(() =>
			{
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

				while (searchPaths.Count != 0)
				{
					if (SuspendUpdateThread)
					{
						SuspendUpdateThreadEvent.WaitOne();
					}

					string searchPath = searchPaths.Dequeue();

					try
					{
						var files = Directory.GetFiles(searchPath, "*.exe", SearchOption.TopDirectoryOnly).Where(x =>
							// Exclude installer executables
							x.IndexOf("redis", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("unins", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("setup", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("patch", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("update", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("install", StringComparison.OrdinalIgnoreCase) < 0 &&
							// Exclude common support executables
							x.IndexOf("report", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("support", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("register", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("activation", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("diagnostics", StringComparison.OrdinalIgnoreCase) < 0 &&
							// Exclude common utility and launcher executables
							x.IndexOf("tool", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("crash", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("config", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("launch", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("plugin", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("benchmark", StringComparison.OrdinalIgnoreCase) < 0 &&
							// Exclude various known executables like SteamVR and CEF
							x.IndexOf("steamvr", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("cefprocess", StringComparison.OrdinalIgnoreCase) < 0 &&
							!x.Contains("svc")).ToArray(); // Services

						if (files.Length != 0)
						{
							const int SPLIT_SIZE = 10;

							for (int i = 0; i < files.Length; i += SPLIT_SIZE)
							{
								Dispatcher.Invoke(new Action<ArraySegment<string>>(arg =>
								{
									foreach (var file in arg)
									{
										if (ProgramListItems.FirstOrDefault(x => x.Path == file) == null)
										{
											ProgramListItems.Add(new ProgramItem(file));
										}
									}
								}), DispatcherPriority.Background, new ArraySegment<string>(files, i, Math.Min(SPLIT_SIZE, files.Length - i)));
							}

							// Give back control to the OS
							Thread.Sleep(10);
						}

						// Continue searching in sub-directories
						var directories = Directory.GetDirectories(searchPath).Where(x =>
							// Avoid deep folder structures
							x.Count(c => c == '\\') < 10 &&
							// Ignore certain folders that are unlikely to contain useful executables
							x.IndexOf("docs", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("cache", StringComparison.OrdinalIgnoreCase) < 0 &&
							x.IndexOf("support", StringComparison.OrdinalIgnoreCase) < 0 &&
							!x.Contains("Data") && // AppData, ProgramData, _Data
							!x.Contains("_CommonRedist") &&
							!x.Contains("__Installer") &&
							!x.Contains("\\$") &&
							!x.Contains("\\.") &&
							!x.Contains("\\Windows") &&
							// Ignore various folders which are known to not contain useful executables
							x.IndexOf("steamvr", StringComparison.OrdinalIgnoreCase) < 0);

						foreach (var path in directories)
						{
							searchPaths.Enqueue(path);
						}
					}
					catch
					{
						// Ignore permission errors
						continue;
					}
				}

				// Hide progress bar after search has finished
				Dispatcher.BeginInvoke(new Action(() => ProgramListProgress.Visibility = Visibility.Collapsed));
			});

			UpdateThread.Start();
		}

		class ProgramItem
		{
			public ProgramItem(string path)
			{
				if (!File.Exists(path))
				{
					throw new FileNotFoundException("Program does not exist", path);
				}

				Path = path;
				Name = FileVersionInfo.GetVersionInfo(path)?.FileDescription;
				if (Name is null || Name.Trim().Length == 0)
				{
					Name = System.IO.Path.GetFileNameWithoutExtension(path);
				}

				Name += " (" + System.IO.Path.GetFileName(path) + ")";

				using (var ico = System.Drawing.Icon.ExtractAssociatedIcon(path))
				{
					Icon = Imaging.CreateBitmapSourceFromHIcon(ico.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());
				}
			}

			public string Name { get; }
			public string Path { get; }
			public ImageSource Icon { get; }
		}

		Thread UpdateThread = null;
		AutoResetEvent SuspendUpdateThreadEvent = new AutoResetEvent(false);
		bool SuspendUpdateThread = false;
		public string FileName { get; private set; }
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
			FileName = (ProgramList.SelectedItem as ProgramItem)?.Path;

			// Only close dialog if an actual item was selected in the list
			if (FileName != null)
			{
				UpdateThread.Abort();
				DialogResult = true;
			}
		}

		void OnConfirmSelection(object sender, MouseButtonEventArgs e)
		{
			OnConfirm(sender, null);
		}
	}
}
