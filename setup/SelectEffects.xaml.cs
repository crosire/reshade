/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;

namespace ReShade.Setup
{
	public class EffectItem : INotifyPropertyChanged
	{
		bool enabled = true;
		public event PropertyChangedEventHandler PropertyChanged;

		public bool Enabled
		{
			get
			{
				return enabled;
			}
			set
			{
				enabled = value;
				PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Enabled)));
			}
		}

		public string Name { get; set; }
		public string Path { get; set; }
	}

	public class EffectRepositoryItem
	{
		public EffectRepositoryItem(string name)
		{
			Name = name;

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			var request = WebRequest.Create("https://api.github.com/repos/" + name + "/contents/Shaders") as HttpWebRequest;
			request.Accept = "application/json";
			request.UserAgent = "reshade";
			request.AutomaticDecompression = DecompressionMethods.GZip;

			try
			{
				using (var response = request.GetResponse() as HttpWebResponse)
				using (Stream stream = response.GetResponseStream())
				using (StreamReader reader = new StreamReader(stream))
				{
					string json = reader.ReadToEnd();

					foreach (Match match in new Regex("\"name\":\"(.*?)\",").Matches(json))
					{
						string filename = match.Groups[1].Value;

						if (Path.GetExtension(filename) == ".fx")
						{
							Effects.Add(new EffectItem
							{
								Name = match.Groups[1].Value,
								Path = name + "/Shaders/" + filename
							});
						}
					}
				}
			}
			catch { }
		}

		public string Name { get; set; }
		public ObservableCollection<EffectItem> Effects { get; set; } = new ObservableCollection<EffectItem>();
	}

	public partial class SelectEffectsDialog : Window
	{
		public SelectEffectsDialog()
		{
			InitializeComponent();

			EffectList.ItemsSource = new List<EffectRepositoryItem> {
				new EffectRepositoryItem("crosire/reshade-shaders"),
			};
		}
		public SelectEffectsDialog(IEnumerable<string> effectFiles)
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

		void OnCheck(object sender, RoutedEventArgs e)
		{
			if (EffectList.Items.Count == 0)
			{
				return;
			}

			var button = sender as Button;

			bool check = (string)button.Content == "Check _all";
			button.Content = check ? "Uncheck _all" : "Check _all";

			if (EffectList.Items[0] is EffectItem)
			{
				foreach (EffectItem item in EffectList.Items)
				{
					item.Enabled = check;
				}
			}
			if (EffectList.Items[0] is EffectRepositoryItem)
			{
				foreach (EffectRepositoryItem parent in EffectList.Items)
				{
					foreach (EffectItem item in parent.Effects)
					{
						item.Enabled = check;
					}
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
	}
}
