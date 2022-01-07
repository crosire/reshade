/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using Microsoft.Win32;
using System.Windows;
using System.Windows.Controls;

namespace ReShade.Setup.Pages
{
	public partial class SelectPresetPage : Page
	{
		public SelectPresetPage()
		{
			InitializeComponent();
		}

		public string FileName { get => PathBox.Text; set => PathBox.Text = value; }

		private void OnBrowseClick(object sender, RoutedEventArgs e)
		{
			var dlg = new OpenFileDialog
			{
				Filter = "Presets|*.ini",
				DefaultExt = ".ini",
				Multiselect = false,
				ValidateNames = true,
				CheckFileExists = true
			};

			if (dlg.ShowDialog() == true)
			{
				FileName = dlg.FileName;
			}
		}
	}
}
