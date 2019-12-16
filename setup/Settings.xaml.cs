using System;
using System.Globalization;
using System.IO;
using System.Windows;
using System.Windows.Data;
using System.Windows.Markup;
using Microsoft.Win32;
using Microsoft.WindowsAPICodePack.Dialogs;

namespace ReShade.Setup
{
	public partial class SettingsWindow
	{
		readonly string configFilePath;

		public SettingsWindow(string configPath)
		{
			InitializeComponent();
			configFilePath = configPath;

			BtnSave.Click += (s, e) => { Save(); Close(); };
			BtnReload.Click += (s, e) => Load();
			BtnCancel.Click += (s, e) => Close();

			Load();
		}

		private void Save()
		{
			var iniFile = new IniFile(configFilePath);

			iniFile.SetValue("GENERAL", "CurrentPresetPath", Preset.Text);
			iniFile.SetValue("GENERAL", "EffectSearchPaths", EffectsPath.Text);
			iniFile.SetValue("GENERAL", "TextureSearchPaths", TexturesPath.Text);
			iniFile.SetValue("GENERAL", "ScreenshotPath", ScreenshotPath.Text);

			iniFile.SetValue("GENERAL", "PerformanceMode", CheckboxValue(PerformanceMode.IsChecked));
			iniFile.SetValue("GENERAL", "ShowFPS", CheckboxValue(ShowFps.IsChecked));
			iniFile.SetValue("GENERAL", "ShowClock", CheckboxValue(ShowClock.IsChecked));

			var skipTut = SkipTut.IsChecked;
			iniFile.SetValue("GENERAL", "TutorialProgress", skipTut.HasValue ? (skipTut.Value ? "4" : "0") : iniFile.GetString("GENERAL", "TutorialProgress", "0"));

			iniFile.Save();
		}

		private void Load()
		{
			if (!File.Exists(configFilePath))
				return;

			var iniFile = new IniFile(configFilePath);

			Preset.Text = iniFile.GetString("GENERAL", "CurrentPresetPath");
			EffectsPath.Text = iniFile.GetString("GENERAL", "EffectSearchPaths");
			TexturesPath.Text = iniFile.GetString("GENERAL", "TextureSearchPaths");
			ScreenshotPath.Text = iniFile.GetString("GENERAL", "ScreenshotPath");

			PerformanceMode.IsChecked = iniFile.GetString("GENERAL", "PerformanceMode") == "1";
			ShowFps.IsChecked = iniFile.GetString("GENERAL", "ShowFPS") == "1";
			ShowClock.IsChecked = iniFile.GetString("GENERAL", "ShowClock") == "1";

			string tutProgress = iniFile.GetString("GENERAL", "TutorialProgress", "0");
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
				SkipTut.ToolTip = "Keep existing tutorial progress";
			}
		}

		private void ChoosePresetDialog(object sender, RoutedEventArgs e)
		{
			string origFirstValue = (Preset.Text ?? "").Split(',')[0];

			var dlg = new OpenFileDialog
			{
				CheckFileExists = false,
				CheckPathExists = true,
				Multiselect = true,
				Filter = "Preset Files (*.ini, *.txt)|*.ini;*.txt",
				DefaultExt = ".ini",
				InitialDirectory = string.IsNullOrWhiteSpace(origFirstValue) ? null : Path.GetDirectoryName(origFirstValue),
				FileName = Path.GetFileName(origFirstValue)
			};

			if (dlg.ShowDialog(this) == true)
			{
				Preset.Text = string.Join(",", dlg.FileNames);
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

		private static string CheckboxValue(bool? check) => (check.HasValue && check.Value ? "1" : "0");
	}
}
