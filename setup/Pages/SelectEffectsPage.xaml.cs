/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
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

		public string PackageName { get; internal set; }
		public string PackageDescription { get; internal set; }

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
		public SelectEffectsPage(IniFile packagesIni)
		{
			InitializeComponent();
			DataContext = this;

			foreach (var package in packagesIni.GetSections())
			{
				bool required = packagesIni.GetString(package, "Required") == "1";
				bool? enabled = required || packagesIni.GetString(package, "Enabled") == "1";

				packagesIni.GetValue(package, "EffectFiles", out string[] packageEffectFiles);
				packagesIni.GetValue(package, "DenyEffectFiles", out string[] packageDenyEffectFiles);

				Items.Add(new EffectPackage
				{
					Selected = enabled,
					Modifiable = !required,
					PackageName = packagesIni.GetString(package, "PackageName"),
					PackageDescription = packagesIni.GetString(package, "PackageDescription"),
					InstallPath = packagesIni.GetString(package, "InstallPath"),
					TextureInstallPath = packagesIni.GetString(package, "TextureInstallPath"),
					DownloadUrl = packagesIni.GetString(package, "DownloadUrl"),
					RepositoryUrl = packagesIni.GetString(package, "RepositoryUrl"),
					EffectFiles = packageEffectFiles.Where(x => packageDenyEffectFiles == null || !packageDenyEffectFiles.Contains(x)).Select(x => new EffectFile { FileName = x, Selected = true }).ToArray(),
					DenyEffectFiles = packageDenyEffectFiles
				});
			}
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

				foreach (var item in Items)
				{
					if (!item.Modifiable)
					{
						continue;
					}

					item.Selected = check;
					item.NotifyPropertyChanged(nameof(item.Selected));
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
