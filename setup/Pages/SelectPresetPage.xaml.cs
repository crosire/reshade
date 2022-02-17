/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;

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
