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
using System.Windows;
using System.Windows.Controls;
using System.Windows.Navigation;

namespace ReShade.Setup.Pages
{
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

		public string[] EffectFiles { get; internal set; }
		public string[] DenyEffectFiles { get; internal set; }

		public string EffectFilesList => EffectFiles != null && EffectFiles.Length != 0 ? string.Join("\n", EffectFiles) : "...";

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

	public partial class SelectPackagesPage : Page
	{
		public SelectPackagesPage(Utilities.IniFile packagesIni, List<string> effectFiles)
		{
			InitializeComponent();
			DataContext = this;

			foreach (var package in packagesIni.GetSections())
			{
				bool required = packagesIni.GetString(package, "Required") == "1";
				bool? enabled = required ? true : (packagesIni.GetString(package, "Enabled") == "1" && effectFiles.Count == 0 ? (bool?)null : false);

				if (packagesIni.GetValue(package, "EffectFiles", out string[] packageEffectFiles))
				{
					if (packageEffectFiles.Intersect(effectFiles).Any())
					{
						enabled = true;
					}
				}

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
					EffectFiles = packageEffectFiles,
					DenyEffectFiles = packageDenyEffectFiles
				});
			}
		}

		public IEnumerable<EffectPackage> SelectedItems => Items.Where(x => x.Selected != false);
		public ObservableCollection<EffectPackage> Items { get; } = new ObservableCollection<EffectPackage>();

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

		private void OnAddPackageButtonClick(object sender, RoutedEventArgs e)
		{
			string url = PathBox.Text;
			if (!url.StartsWith("http") || !url.EndsWith(".zip"))
			{
				// Only accept ZIP download links
				return;
			}

			PathBox.Text = string.Empty;

			Items.Add(new EffectPackage
			{
				Selected = true,
				PackageName = Path.GetFileName(url),
				InstallPath = ".\\reshade-shaders\\Shaders",
				TextureInstallPath = ".\\reshade-shaders\\Textures",
				DownloadUrl = url,
				RepositoryUrl = url
			});
		}

		private void OnTextBoxGotFocus(object sender, RoutedEventArgs e)
		{
			if (sender is TextBox tb)
			{
				tb.Dispatcher.BeginInvoke(new Action(tb.SelectAll));
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
