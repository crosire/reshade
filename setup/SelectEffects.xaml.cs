/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;

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

		public string Name { get; set; }
		public ObservableCollection<EffectItem> Effects { get; set; } = new ObservableCollection<EffectItem>();
	}

	public partial class SelectEffectsDialog : Window
	{
		public SelectEffectsDialog()
		{
			InitializeComponent();

			try
			{
				// Add default repository
				Repositories.Add(new EffectRepositoryItem(
					CustomRepositoryName.Text = "crosire/reshade-shaders"));
			}
			catch { }

			EffectList.ItemsSource = Repositories;
		}

		ObservableCollection<EffectRepositoryItem> Repositories = new ObservableCollection<EffectRepositoryItem>();

		void OnCheck(object sender, RoutedEventArgs e)
		{
			if (EffectList.Items.Count == 0)
			{
				return;
			}

			var button = sender as Button;

			bool check = (string)button.Content == "Check _all";
			button.Content = check ? "Uncheck _all" : "Check _all";

			foreach (EffectRepositoryItem repository in Repositories)
			{
				foreach (EffectItem item in repository.Effects)
				{
					item.Enabled = check;
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

		void OnAddRepository(object sender, RoutedEventArgs e)
		{
			if (CustomRepositoryName.Text.IndexOf('/') < 0)
			{
				CustomRepositoryName.Foreground = Brushes.Red;
				return;
			}

			// Check if this repository was already added
			if (Repositories.Where(x => x.Name == CustomRepositoryName.Text).Count() == 0)
			{
				EffectRepositoryItem repository = null;
				try
				{
					repository = new EffectRepositoryItem(CustomRepositoryName.Text);
				}
				catch
				{
					// Invalid repository name
					CustomRepositoryName.Foreground = Brushes.Red;
					return;
				}

				Repositories.Add(repository);
			}

			CustomRepositoryName.Text = string.Empty;
			CustomRepositoryName.Foreground = Brushes.Black;
		}
	}
}
