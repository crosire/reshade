/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ReShade.Setup.Dialogs
{
	public class EffectFile : INotifyPropertyChanged
	{
		public bool Enabled { get; set; }
		public string FileName { get; set; }
		public string FilePath { get; set; }

		public event PropertyChangedEventHandler PropertyChanged;
		internal void NotifyPropertyChanged(string propertyName)
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
		}
	}

	public partial class SelectEffectsDialog : Window
	{
		public SelectEffectsDialog(string packageName, IEnumerable<string> files)
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
					Enabled = enabled,
					FileName = Path.GetFileName(path),
					FilePath = path
				});
			}

			// Enable all items if none is enabled (which indicates that the package was not previously installed yet)
			if (!isAnyEnabled)
			{
				foreach (var item in Items)
				{
					item.Enabled = true;
				}
			}
		}

		public IEnumerable<EffectFile> EnabledItems => Items.Where(x => x.Enabled);
		public ObservableCollection<EffectFile> Items { get; } = new ObservableCollection<EffectFile>();

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
					item.Enabled = check;
					item.NotifyPropertyChanged(nameof(item.Enabled));
				}
			}
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
