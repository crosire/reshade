/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;
using Microsoft.Win32;
using ReShade.Setup.Utilities;

namespace ReShade.Setup.Pages
{
	public class EffectFile : INotifyPropertyChanged
	{
		public bool Selected { get; set; }

		public string FileName { get; internal set; }

		public event PropertyChangedEventHandler PropertyChanged;

		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}
	public class EffectPackage : INotifyPropertyChanged
	{
		public bool? Selected { get; set; } = false;
		public bool  Modifiable { get; set; } = true;

		public string Name { get; internal set; }
		public string Description { get; internal set; }

		public string InstallPath { get; internal set; }
		public string TextureInstallPath { get; internal set; }
		public string DownloadUrl { get; internal set; }
		public string RepositoryUrl { get; internal set; }

		public EffectFile[] EffectFiles { get; internal set; }
		public string[] DenyEffectFiles { get; internal set; }

		public event PropertyChangedEventHandler PropertyChanged;

		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}

	public class EffectPackageCheckBox : CheckBox
	{
		protected override void OnToggle()
		{
			// Change cycle order to show filled out box first
			if (IsChecked != false)
			{
				IsChecked = new bool?(!IsChecked.HasValue);
			}
			else
			{
				IsChecked = IsThreeState ? null : new bool?(true);
			}
		}
	}

	public partial class SelectEffectsPage : Page
	{
		public SelectEffectsPage()
		{
			InitializeComponent();
			DataContext = this;

			Task.Run(() =>
			{
				// Attempt to download effect package list
				using (var client = new WebClient())
				{
					// Ensure files are downloaded again if they changed
					client.CachePolicy = new System.Net.Cache.RequestCachePolicy(System.Net.Cache.RequestCacheLevel.Revalidate);

					try
					{
						using (Stream packagesStream = client.OpenRead("https://raw.githubusercontent.com/crosire/reshade-shaders/list/EffectPackages.ini"))
						{
							var packagesIni = new IniFile(packagesStream);

							foreach (string package in packagesIni.GetSections())
							{
								bool required = packagesIni.GetString(package, "Required") == "1";
								bool? enabled = required || packagesIni.GetString(package, "Enabled") == "1";

								packagesIni.GetValue(package, "EffectFiles", out string[] packageEffectFiles);
								packagesIni.GetValue(package, "DenyEffectFiles", out string[] packageDenyEffectFiles);

								var item = new EffectPackage
								{
									Selected = enabled,
									Modifiable = !required,
									Name = packagesIni.GetString(package, "PackageName"),
									Description = packagesIni.GetString(package, "PackageDescription"),
									InstallPath = packagesIni.GetString(package, "InstallPath", string.Empty),
									TextureInstallPath = packagesIni.GetString(package, "TextureInstallPath", string.Empty),
									DownloadUrl = packagesIni.GetString(package, "DownloadUrl"),
									RepositoryUrl = packagesIni.GetString(package, "RepositoryUrl"),
									EffectFiles = packageEffectFiles?.Where(x => packageDenyEffectFiles == null || !packageDenyEffectFiles.Contains(x)).Select(x => new EffectFile { FileName = x, Selected = true }).ToArray(),
									DenyEffectFiles = packageDenyEffectFiles
								};

								Dispatcher.Invoke(() => { Items.Add(item); });
							}
						}
					}
					catch (WebException ex)
					{
						// Ignore if this list failed to download, since setup can still proceed without it
						Dispatcher.Invoke(() =>
						{
							MessageBox.Show("Failed to download list of available effects:\n" + ex.Message + "\n\nTry using a proxy or VPN and verify that you can access https://raw.githubusercontent.com.", "Warning", MessageBoxButton.OK, MessageBoxImage.Exclamation);
						});
					}
				}

				// Update selection after all items were added
				Dispatcher.Invoke(() => { PresetPath = PresetPath; });
			});	
		}

		public IEnumerable<EffectPackage> SelectedItems => Items.Where(x => x.Selected != false);
		public ObservableCollection<EffectPackage> Items { get; } = new ObservableCollection<EffectPackage>();

		public string PresetPath
		{
			get => PresetPathBox.Text;
			set
			{
				PresetPathBox.Text = value;

				if (string.IsNullOrEmpty(value))
				{
					return;
				}

				var preset = new IniFile(value);

				if (preset.GetValue(string.Empty, "Techniques", out string[] techniques) == false)
				{
					return;
				}

				var effectFiles = new List<string>();

				foreach (string technique in techniques)
				{
					var filenameIndex = technique.IndexOf('@');
					if (filenameIndex > 0)
					{
						string filename = technique.Substring(filenameIndex + 1);

						effectFiles.Add(filename);
					}
				}

				foreach (EffectPackage package in Items)
				{
					if (!package.Modifiable)
					{
						continue;
					}

					bool anyEffectFileUsed = false;

					foreach (EffectFile effectFile in package.EffectFiles)
					{
						effectFile.Selected = effectFiles.Contains(effectFile.FileName);
						effectFile.NotifyPropertyChanged(nameof(effectFile.Selected));

						if (effectFile.Selected)
						{
							anyEffectFileUsed = true;
						}
					}

					package.Selected = anyEffectFileUsed ? (bool?)null : false;
					package.NotifyPropertyChanged(nameof(package.Selected));
				}
			}
		}

		private void OnCheckAllClick(object sender, RoutedEventArgs e)
		{
			if (Items.Count == 0)
			{
				return;
			}

			if (sender is Button button)
			{
				const string CHECK_LABEL = "Check _all";
				const string UNCHECK_LABEL = "Uncheck _all";

				bool check = button.Content as string == CHECK_LABEL;
				button.Content = check ? UNCHECK_LABEL : CHECK_LABEL;

				foreach (EffectPackage package in Items)
				{
					if (!package.Modifiable)
					{
						continue;
					}

					package.Selected = check;
					package.NotifyPropertyChanged(nameof(package.Selected));
				}
			}
		}

		private void OnBrowsePresetClick(object sender, RoutedEventArgs e)
		{
			var dlg = new OpenFileDialog
			{
				Filter = "Presets|*.ini;*.txt",
				DefaultExt = ".ini",
				Multiselect = false,
				ValidateNames = true,
				CheckFileExists = true
			};

			if (dlg.ShowDialog() == true)
			{
				PresetPath = dlg.FileName;
			}
		}

		private void OnHyperlinkRequestNavigate(object sender, RequestNavigateEventArgs e)
		{
			try
			{
				Process.Start(e.Uri.AbsoluteUri);
				e.Handled = true;
			}
			catch
			{
				e.Handled = false;
			}
		}
	}
}
