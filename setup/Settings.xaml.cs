using System;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Markup;
using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;

namespace ReShade.Setup
{
	public partial class SettingsWindow
	{
		private const string IniSectionName = "GENERAL";

		private readonly string _configFilePath;

		public SettingsWindow(string configPath)
		{
			InitializeComponent();
			_configFilePath = configPath;

			Presets.TextChanged += (s, e) =>
			{
				PresetOptions.Collection = (Presets.Text ?? "").Split(',')
					.Where(f => !string.IsNullOrWhiteSpace(f))
					.Select((f, i) => new PresetComboItem { Text = Path.GetFileName(f), Value = i });
			};

			BtnSave.Click += (s, e) => { Save(); Close(); };
			BtnReload.Click += (s, e) => Load();
			BtnCancel.Click += (s, e) => Close();

			Load();
		}

		private void WriteValue(string key, string value)
		{
			IniFile.WriteValue(_configFilePath, IniSectionName, key, NullIfBlank(value));
		}

		private string ReadValue(string key, string defaultValue = null)
		{
			string value = IniFile.ReadValue(_configFilePath, IniSectionName, key, defaultValue);
			return NullIfBlank(value) ?? defaultValue;
		}

		private void Save()
		{
			WriteValue("PresetFiles", Presets.Text);
			WriteValue("CurrentPreset", PresetSelect.SelectedValue?.ToString());
			WriteValue("EffectSearchPaths", EffectsPath.Text);
			WriteValue("TextureSearchPaths", TexturesPath.Text);
			WriteValue("ScreenshotPath", ScreenshotsPath.Text);

			WriteValue("PerformanceMode", CheckboxValue(PerformanceMode.IsChecked));
			WriteValue("ShowFPS", CheckboxValue(ShowFps.IsChecked));
			WriteValue("ShowClock", CheckboxValue(ShowClock.IsChecked));

			string tutProgress = ReadValue("TutorialProgress", "0");
			var skipTut = SkipTut.IsChecked;
			WriteValue("TutorialProgress", skipTut.HasValue ? (skipTut.Value ? "4" : "0") : tutProgress);
		}

		private void Load()
		{
			if (File.Exists(_configFilePath))
			{
				Presets.Text = ReadValue("PresetFiles");
				PresetSelect.SelectedIndex = Convert.ToInt32(ReadValue("CurrentPreset", "-1")) + 1;
				EffectsPath.Text = ReadValue("EffectSearchPaths");
				TexturesPath.Text = ReadValue("TextureSearchPaths");
				ScreenshotsPath.Text = ReadValue("ScreenshotPath");

				PerformanceMode.IsChecked = ReadValue("PerformanceMode") == "1";
				ShowFps.IsChecked = ReadValue("ShowFPS") == "1";
				ShowClock.IsChecked = ReadValue("ShowClock") == "1";

				string tutProgress = ReadValue("TutorialProgress", "0");
				if (tutProgress == "0" || tutProgress == "4")
				{
					SkipTut.IsThreeState = false;
					SkipTut.IsChecked = tutProgress == "4";
					SkipTut.ToolTip = null;
				}
				else
				{
					SkipTut.IsThreeState = true;
					SkipTut.IsChecked = null;
					SkipTut.ToolTip = "Neutral means keep pre-existing progress";
				}
			}
		}

		private void BtnPresets_Clicked(object sender, RoutedEventArgs e)
		{
			string origFirstValue = (Presets.Text ?? "").Split(',')[0];

			var dlg = new OpenFileDialog
			{
				CheckFileExists = false,
				CheckPathExists = true,
				Multiselect = true,
				Filter = "Config Files (*.ini, *.txt)|*.ini;*.txt",
				DefaultExt = ".ini",
				InitialDirectory = string.IsNullOrWhiteSpace(origFirstValue) ? null : Path.GetDirectoryName(origFirstValue),
				FileName = Path.GetFileName(origFirstValue)
			};

			if (dlg.ShowDialog(this) == true)
			{
				Presets.Text = string.Join(",", dlg.FileNames);
			}
		}

		private void ChooseFolderDialog(object sender, RoutedEventArgs e)
		{
			var target = e.Source as FrameworkElement;
			string origFirstValue = (target.Tag as string ?? "").Split(',')[0].TrimEnd(Path.PathSeparator);

			var dlg = new CommonOpenFileDialog
			{
				IsFolderPicker = true,
				Multiselect = true,
				EnsureFileExists = true,
				InitialDirectory = string.IsNullOrWhiteSpace(origFirstValue) ? null : Path.GetDirectoryName(origFirstValue),
				DefaultFileName = Path.GetFileName(origFirstValue)
			};

			if (dlg.ShowDialog() == CommonFileDialogResult.Ok)
			{
				target.Tag = string.Join(",", dlg.FileNames);
			}
		}

		// Force select the first item when there is no selection
		private void PresetSelect_SelectionChanged(object sender, SelectionChangedEventArgs e)
		{
			PresetSelect.SelectedItem = PresetSelect.SelectedItem ?? PresetSelect.Items[0];
		}

		private static string CheckboxValue(bool? check) => (check.HasValue && check.Value ? "1" : "0");

		private static string NullIfBlank(string s) => string.IsNullOrWhiteSpace(s) ? null : s;
	}

	public struct PresetComboItem
	{
		public string Text { get; set; }
		public int? Value { get; set; }
	}

	[ValueConversion(typeof(int), typeof(Visibility))]
	public class ComboSizeToVisibilityConverter : MarkupExtension, IValueConverter
	{
		public override object ProvideValue(IServiceProvider serviceProvider)
		{
			return this;
		}

		public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
		{
			return (System.Convert.ToInt32(value) > 1 ? Visibility.Visible : Visibility.Collapsed);
		}

		public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
		{
			throw new NotImplementedException();
		}
	}
}
