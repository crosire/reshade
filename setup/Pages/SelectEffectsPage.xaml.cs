/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ReShade.Setup.Pages
{
	public class EffectFile : INotifyPropertyChanged
	{
		public bool Selected { get; set; }

		public string FileName { get; internal set; }
		public string FilePath { get; internal set; }

		public event PropertyChangedEventHandler PropertyChanged;

		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}

	public partial class SelectEffectsPage : Page
	{
		public IEnumerable<EffectFile> SelectedItems => Items.Where(x => x.Selected);
		public ObservableCollection<EffectFile> Items { get; } = new ObservableCollection<EffectFile>();

		public SelectEffectsPage(string packageName, IEnumerable<string> files)
		{
			InitializeComponent();
			DataContext = this;

			// Remove any author description from display name
			int authorIndex = packageName.IndexOf(" by ");
			if (authorIndex != -1)
			{
				packageName = packageName.Remove(authorIndex);
			}

			// Put package name in quotes in the tile, so repeating words do not look odd
			PackageName.Text = '\"' + packageName + '\"';

			var isAnyEnabled = false;
			foreach (var path in files)
			{
				bool enabled = File.Exists(path);
				isAnyEnabled = isAnyEnabled || enabled;

				Items.Add(new EffectFile
				{
					Selected = enabled,
					FileName = Path.GetFileName(path),
					FilePath = path
				});
			}

			// Enable all items if none is enabled (which indicates that the package was not previously installed yet)
			if (!isAnyEnabled)
			{
				foreach (var item in Items)
				{
					item.Selected = true;
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
					item.Selected = check;
					item.NotifyPropertyChanged(nameof(item.Selected));
				}
			}
		}

		private void OnCheckBoxMouseEnter(object sender, MouseEventArgs e)
		{
			if (e.LeftButton == MouseButtonState.Pressed && sender is CheckBox checkbox)
			{
				checkbox.IsChecked = !checkbox.IsChecked;
			}
		}
		private void OnCheckBoxMouseCapture(object sender, MouseEventArgs e)
		{
			if (e.LeftButton == MouseButtonState.Pressed && sender is CheckBox checkbox)
			{
				checkbox.IsChecked = !checkbox.IsChecked;
				checkbox.ReleaseMouseCapture();
			}
		}
	}
}
