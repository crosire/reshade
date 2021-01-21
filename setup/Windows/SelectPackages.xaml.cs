/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
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
using System.Windows.Input;
using System.Windows.Navigation;

namespace ReShade.Setup.Dialogs
{
	public class EffectPackage : INotifyPropertyChanged
	{
		public bool? Enabled { get; set; } = false;
		public bool Modifiable { get; set; } = true;
		public string PackageName { get; set; }
		public string PackageDescription { get; set; }
		public string InstallPath { get; set; }
		public string TextureInstallPath { get; set; }
		public string DownloadUrl { get; set; }
		public string RepositoryUrl { get; set; }

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

	public partial class SelectPackagesDialog : Window
	{
		public SelectPackagesDialog(Utilities.IniFile packagesIni)
		{
			InitializeComponent();
			DataContext = this;

			foreach (var package in packagesIni.GetSections())
			{
				bool enabled = packagesIni.GetString(package, "Enabled") == "1";
				bool required = packagesIni.GetString(package, "Required") == "1";

				Items.Add(new EffectPackage
				{
					Enabled = required ? true : enabled ? (bool?)null : false,
					Modifiable = !required,
					PackageName = packagesIni.GetString(package, "PackageName"),
					PackageDescription = packagesIni.GetString(package, "PackageDescription"),
					InstallPath = packagesIni.GetString(package, "InstallPath"),
					TextureInstallPath = packagesIni.GetString(package, "TextureInstallPath"),
					DownloadUrl = packagesIni.GetString(package, "DownloadUrl"),
					RepositoryUrl = packagesIni.GetString(package, "RepositoryUrl")
				});
			}
		}

		public IEnumerable<EffectPackage> EnabledItems => Items.Where(x => x.Enabled != false);
		public ObservableCollection<EffectPackage> Items { get; } = new ObservableCollection<EffectPackage>();

		void OnCheck(object sender, RoutedEventArgs e)
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

					item.Enabled = check;
					item.NotifyPropertyChanged(nameof(item.Enabled));
				}
			}
		}

		void OnCancel(object sender, RoutedEventArgs e)
		{
			DialogResult = false;
		}
		void OnConfirm(object sender, RoutedEventArgs e)
		{
			DialogResult = true;
		}

		void OnAddPackage(object sender, RoutedEventArgs e)
		{
			string url = PathBox.Text;
			if (!url.StartsWith("http") || !url.EndsWith(".zip"))
			{
				// Only accept ZIP download links
				return;
			}

			PathBox.Text = string.Empty;

			Items.Add(new EffectPackage {
				Enabled = true,
				PackageName = Path.GetFileName(url),
				InstallPath = ".\\reshade-shaders\\Shaders",
				TextureInstallPath = ".\\reshade-shaders\\Textures",
				DownloadUrl = url,
				RepositoryUrl = url
			});
		}
		void OnTextBoxGotFocus(object sender, RoutedEventArgs e)
		{
			var tb = (TextBox)sender;
			tb.Dispatcher.BeginInvoke(new Action(() => tb.SelectAll()));
		}

		void OnHyperlinkRequestNavigate(object sender, RequestNavigateEventArgs e)
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
