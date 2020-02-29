/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

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
		public SelectEffectsDialog()
		{
			InitializeComponent();
			DataContext = this;

			Packages.Add(new EffectPackage {
				Enabled = true,
				PackageName = "Standard effects",
				PackageDescription = "https://github.com/crosire/reshade-shaders",
				DownloadUrl = "https://github.com/crosire/reshade-shaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "Color effects by prod80",
				PackageDescription = "https://github.com/prod80/prod80-ReShade-Repository",
				DownloadUrl = "https://github.com/prod80/prod80-ReShade-Repository/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "qUINT by Marty McFly",
				PackageDescription = "https://github.com/martymcmodding/qUINT",
				DownloadUrl = "https://github.com/martymcmodding/qUINT/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "Depth3D by BlueSkyDefender",
				PackageDescription = "https://github.com/BlueSkyDefender/Depth3D",
				DownloadUrl = "https://github.com/BlueSkyDefender/Depth3D/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "OtisFX by Otis",
				PackageDescription = "https://github.com/FransBouma/OtisFX",
				DownloadUrl = "https://github.com/FransBouma/OtisFX/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "luluco250's effects",
				PackageDescription = "https://github.com/luluco250/FXShaders",
				DownloadUrl = "https://github.com/luluco250/FXShaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "Fubax ReShade FX shaders",
				PackageDescription = "https://github.com/Fubaxiusz/fubax-shaders",
				DownloadUrl = "https://github.com/Fubaxiusz/fubax-shaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "Daodan's effects",
				PackageDescription = "https://github.com/Daodan317081/reshade-shaders",
				DownloadUrl = "https://github.com/Daodan317081/reshade-shaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "brussell's effects",
				PackageDescription = "https://github.com/brussell1/Shaders",
				DownloadUrl = "https://github.com/brussell1/Shaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "Pirate Shaders by Heathen",
				PackageDescription = "https://github.com/Heathen/Pirate-Shaders",
				DownloadUrl = "https://github.com/Heathen/Pirate-Shaders/archive/master.zip"
			});
			Packages.Add(new EffectPackage {
				PackageName = "PPFX by Euda",
				PackageDescription = "https://github.com/pascalmatthaeus/ppfx",
				DownloadUrl = "https://github.com/pascalmatthaeus/ppfx/archive/master.zip"
			});
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
