/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Windows.Controls;
using System.Windows.Navigation;

namespace ReShade.Setup.Pages
{
	public class Addon : INotifyPropertyChanged
	{
		public bool Enabled => !string.IsNullOrEmpty(MainWindow.is64Bit ? DownloadUrl64 : DownloadUrl32);
		public bool Selected { get; set; } = false;

		public string Name { get; internal set; }
		public string Description { get; internal set; }

		public string DownloadUrl => MainWindow.is64Bit ? DownloadUrl64 : DownloadUrl32;
		public string DownloadUrl32 { get; internal set; }
		public string DownloadUrl64 { get; internal set; }
		public string RepositoryUrl { get; internal set; }

		public event PropertyChangedEventHandler PropertyChanged;

		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}

	public partial class SelectAddonsPage : Page
	{
		public SelectAddonsPage(Utilities.IniFile addonsIni)
		{
			InitializeComponent();
			DataContext = this;

			foreach (var addon in addonsIni.GetSections())
			{
				Items.Add(new Addon
				{
					Name = addonsIni.GetString(addon, "Name"),
					Description = addonsIni.GetString(addon, "Description"),
					DownloadUrl32 = addonsIni.GetString(addon, "DownloadUrl32"),
					DownloadUrl64 = addonsIni.GetString(addon, "DownloadUrl64"),
					RepositoryUrl = addonsIni.GetString(addon, "RepositoryUrl")
				});
			}
		}

		public IEnumerable<Addon> SelectedItems => Items.Where(x => x.Selected);
		public ObservableCollection<Addon> Items { get; } = new ObservableCollection<Addon>();

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
