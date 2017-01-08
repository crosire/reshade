using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;
using System.ComponentModel;
using System.IO;
using System.Windows;

namespace ReShade.Setup
{
	/// <summary>
	/// Interaction logic for ConfigEditor.xaml
	/// </summary>
	public partial class ConfigEditor : Window
	{
		private const string INI_SECTION = "GENERAL";

		private string _configFilePath;

		public ConfigEditor(string configPath)
		{
			InitializeComponent();
			_configFilePath = configPath;

			_load();
			Closing += (object sender, CancelEventArgs args) => _save();
		}

		private void _writeValue(string key, string value)
		{
			IniFile.WriteValue(_configFilePath, INI_SECTION, key, value);
		}

		private string _readValue(string key, string defaultValue = null)
		{
			return IniFile.ReadValue(_configFilePath, INI_SECTION, key, defaultValue);
		}

		private void _save()
		{
			_writeValue("PresetFiles", Presets.Text);
			_writeValue("EffectSearchPaths", EffectsPath.Text);
			_writeValue("TextureSearchPaths", TexturesPath.Text);
			_writeValue("ScreenshotPath", ScreenshotsPath.Text);

			_writeValue("PerformanceMode", _checkboxValue(PerformanceMode.IsChecked));
			_writeValue("ShowFPS", _checkboxValue(ShowFPS.IsChecked));
			_writeValue("ShowClock", _checkboxValue(ShowClock.IsChecked));

			var tutProgress = _readValue("TutorialProgress", "0");
			var skipTut = SkipTut.IsChecked;
			_writeValue("TutorialProgress", skipTut.HasValue ? (skipTut.Value ? "4" : "0") : tutProgress);
		}

		private void _load()
		{
			if (File.Exists(_configFilePath))
			{
				Presets.Text = _readValue("PresetFiles");
				EffectsPath.Text = _readValue("EffectSearchPaths");
				TexturesPath.Text = _readValue("TextureSearchPaths");
				ScreenshotsPath.Text = _readValue("ScreenshotPath");

				PerformanceMode.IsChecked = _readValue("PerformanceMode") == "1";
				ShowFPS.IsChecked = _readValue("ShowFPS") == "1";
				ShowClock.IsChecked = _readValue("ShowClock") == "1";

				var tutProgress = _readValue("TutorialProgress", "0");
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

		private string _checkboxValue(bool? check) => (check.HasValue && check.Value ? "1" : "0");

		private void btnPreset_Clicked(object sender, RoutedEventArgs e)
		{
			var origFirstValue = (Presets.Text ?? "").Split(',')[0];

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

			var result = dlg.ShowDialog(this);
			if (result.HasValue && result.Value)
			{
				Presets.Text = string.Join(",", dlg.FileNames);
			}
		}

		private void _chooseFolderDialog(object sender, RoutedEventArgs e)
		{
			var target = e.Source as FrameworkElement;
			var origFirstValue = (target.Tag as string ?? "").Split(',')[0];

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
	}
}
