/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ReShade.Setup
{
	public class EffectPackage : INotifyPropertyChanged
	{
		public bool Enabled { get; set; }
		public string PackageName { get; set; }
		public string PackageDescription { get; set; }
		public string InstallPath { get; set; }
		public string TextureInstallPath { get; set; }
		public string DownloadUrl { get; set; }

		public event PropertyChangedEventHandler PropertyChanged;
		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}

	public partial class SelectEffectsDialog : Window
	{
		public SelectEffectsDialog(Utilities.IniFile packagesIni)
		{
			InitializeComponent();
			DataContext = this;

			foreach (var package in packagesIni.GetSections())
			{
				Packages.Add(new EffectPackage
				{
					Enabled = packagesIni.GetString(package, "Enabled") == "1",
					PackageName = packagesIni.GetString(package, "PackageName"),
					PackageDescription = packagesIni.GetString(package, "PackageDescription"),
					InstallPath = packagesIni.GetString(package, "InstallPath"),
					TextureInstallPath = packagesIni.GetString(package, "TextureInstallPath"),
					DownloadUrl = packagesIni.GetString(package, "DownloadUrl")
				});
			}
		}

		public IEnumerable<EffectPackage> EnabledPackages => Packages.Where(x => x.Enabled);
		public ObservableCollection<EffectPackage> Packages { get; } = new ObservableCollection<EffectPackage>();

		void OnCheck(object sender, RoutedEventArgs e)
		{
			if (Packages.Count == 0)
			{
				return;
			}

			if (sender is Button button)
			{
				const string CHECK_LABEL = "Check _all";
				const string UNCHECK_LABEL = "Uncheck _all";

				bool check = button.Content as string == CHECK_LABEL;
				button.Content = check ? UNCHECK_LABEL : CHECK_LABEL;

				foreach (var package in Packages)
				{
					package.Enabled = check;
					package.NotifyPropertyChanged(nameof(package.Enabled));
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

		void OnCheckBoxMouseEnter(object sender, MouseEventArgs e)
		{
			if (e.LeftButton == MouseButtonState.Pressed && sender is CheckBox checkbox)
			{
				checkbox.IsChecked = !checkbox.IsChecked;
			}
		}

		void OnCheckBoxMouseCapture(object sender, MouseEventArgs e)
		{
			if (e.LeftButton == MouseButtonState.Pressed && sender is CheckBox checkbox)
			{
				checkbox.IsChecked = !checkbox.IsChecked;
				checkbox.ReleaseMouseCapture();
			}
		}
	}
}
