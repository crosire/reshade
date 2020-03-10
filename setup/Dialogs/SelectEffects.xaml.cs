/**
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

			var packages = new List<EffectPackage>();
			foreach (var packageName in packagesIni.GetSections())
			{
				packages.Add(new EffectPackage
				{
					Enabled = packagesIni.GetString(packageName, "Enabled") == "1",
					PackageName = packageName,
					PackageDescription = packagesIni.GetString(packageName, "PackageDescription"),
					DownloadUrl = packagesIni.GetString(packageName, "DownloadUrl")
				});
			}

			// Show default enabled packages at the top
			packages.Sort((lhs, rhs) => (lhs.Enabled == rhs.Enabled ? 100 : 0) + rhs.PackageName.CompareTo(lhs.PackageName));
			packages.ForEach(x => Packages.Add(x));
		}

		public string[] EnabledPackageUrls => Packages.Where(x => x.Enabled).Select(x => x.DownloadUrl).ToArray();
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
