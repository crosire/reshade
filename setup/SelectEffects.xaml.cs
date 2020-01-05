/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Configuration;
using System.IO;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace ReShade.Setup
{
	public class EffectItem : INotifyPropertyChanged
	{
		bool enabled = true;
		internal EffectRepositoryItem Parent = null;
		public event PropertyChangedEventHandler PropertyChanged;

		public bool? Enabled
		{
			get
			{
				return enabled;
			}
			set
			{
				enabled = value ?? false;
				NotifyPropertyChanged();
				Parent.NotifyPropertyChanged();
			}
		}

		public string Name { get; set; }
		public string Path { get; set; }

		internal void NotifyPropertyChanged()
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Enabled)));
		}
	}

	public class EffectRepositoryItem : INotifyPropertyChanged
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
							Path = name + "/Shaders/" + filename,
							Parent = this
						});
					}
				}
			}
		}

		public event PropertyChangedEventHandler PropertyChanged;

		public bool? Enabled
		{
			get
			{
				int count = Effects.Where(x => x.Enabled.Value).Count();
				return count == Effects.Count ? true : count == 0 ? false : (bool?)null;
			}
			set
			{
				foreach (var item in Effects)
				{
					item.Enabled = value ?? false;
				}
			}
		}

		public string Name { get; set; }
		public ObservableCollection<EffectItem> Effects { get; set; } = new ObservableCollection<EffectItem>();

		internal void NotifyPropertyChanged()
		{
			PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Enabled)));
		}
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

				foreach (string repository in ConfigurationManager.AppSettings["repositories"].Split(','))
				{
					Repositories.Add(new EffectRepositoryItem(repository));
				}
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

				// Add repository to configuration file, so that it is remembered for the next time
				var configFile = ConfigurationManager.OpenExeConfiguration(ConfigurationUserLevel.None);
				var configSettings = configFile.AppSettings.Settings;

				if (configSettings["repositories"] == null)
				{
					configSettings.Add("repositories", repository.Name);
				}
				else
				{
					configSettings["repositories"].Value += "," + repository.Name;
				}

				configFile.Save(ConfigurationSaveMode.Modified);
			}

			CustomRepositoryName.Text = string.Empty;
			CustomRepositoryName.Foreground = Brushes.Black;
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
