/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace ReShade.Setup
{
	public partial class SelectWindow : Window
	{
		public class EffectItem : INotifyPropertyChanged
		{
			public string Name { get; set; }
			public string Path { get; set; }

			public bool IsChecked
			{
				get
				{
					return _isChecked;
				}
				set
				{
					_isChecked = value;
					PropertyChanged?.Invoke(this, new PropertyChangedEventArgs("IsChecked"));
				}
			}

			private bool _isChecked = true;
			public event PropertyChangedEventHandler PropertyChanged;
		}

		public SelectWindow(IEnumerable<string> effectFiles)
		{
			InitializeComponent();

			EffectList.ItemsSource =
				effectFiles
					.Where(it => Path.GetExtension(it) == ".fx")
					.Select(it => new EffectItem {
						Name = Path.GetFileName(it),
						Path = it
					});
		}

		public IEnumerable<EffectItem> GetSelection()
		{
			return EffectList.Items.Cast<EffectItem>();
		}

		private void ChangeChecked(object sender, RoutedEventArgs e)
		{
			var button = (Button)sender;
			bool check = (string)button.Content == "Check All";
			button.Content = check ? "Uncheck All" : "Check All";

			foreach (EffectItem item in EffectList.Items)
			{
				item.IsChecked = check;
			}
		}
		private void ConfirmSelection(object sender, RoutedEventArgs e)
		{
			Close();
		}
	}
}
