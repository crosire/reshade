/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using ReShade.Setup.Utilities;
using System.IO;
using System.Windows;

namespace ReShade.Setup.Dialogs
{
	public partial class SettingsDialog
	{
		readonly string configFilePath;

		public SettingsDialog(string configPath)
		{
			InitializeComponent();
			configFilePath = configPath;

			OnReload(this, null);
		}

		void OnSave(object sender, RoutedEventArgs e)
		{
			var iniFile = new IniFile(configFilePath);

			iniFile.SetValue("GENERAL", "PresetPath", Preset.Text);
			iniFile.SetValue("GENERAL", "PerformanceMode", CheckboxValue(PerformanceMode.IsChecked));
			iniFile.SetValue("GENERAL", "EffectSearchPaths", EffectsPath.Text);
			iniFile.SetValue("GENERAL", "TextureSearchPaths", TexturesPath.Text);

			iniFile.SetValue("OVERLAY", "ShowFPS", CheckboxValue(ShowFps.IsChecked));
			iniFile.SetValue("OVERLAY", "ShowClock", CheckboxValue(ShowClock.IsChecked));

			var skipTut = SkipTut.IsChecked;
			iniFile.SetValue("OVERLAY", "TutorialProgress", skipTut.HasValue ?
				(skipTut.Value ? "4" : "0") :
				iniFile.GetString("OVERLAY", "TutorialProgress", "0"));

			iniFile.SetValue("SCREENSHOTS", "SavePath", ScreenshotPath.Text);

			iniFile.SaveFile();

			DialogResult = true;
		}

		void OnCancel(object sender, RoutedEventArgs e)
		{
			DialogResult = false;
		}

		void OnReload(object sender, RoutedEventArgs e)
		{
			if (!File.Exists(configFilePath))
				return;

			var iniFile = new IniFile(configFilePath);

			Preset.Text = iniFile.GetString("GENERAL", "PresetPath");
			EffectsPath.Text = iniFile.GetString("GENERAL", "EffectSearchPaths");
			TexturesPath.Text = iniFile.GetString("GENERAL", "TextureSearchPaths");
			ScreenshotPath.Text = iniFile.GetString("SCREENSHOTS", "SavePath");

			PerformanceMode.IsChecked = iniFile.GetString("GENERAL", "PerformanceMode") == "1";
			ShowFps.IsChecked = iniFile.GetString("OVERLAY", "ShowFPS") == "1";
			ShowClock.IsChecked = iniFile.GetString("OVERLAY", "ShowClock") == "1";

			var tutProgress = iniFile.GetString("OVERLAY", "TutorialProgress", "0");
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

		void OnChoosePresetDialog(object sender, RoutedEventArgs e)
		{
			var dlg = new FileOpenDialog
			{
				CheckPathExists = true,
				CheckFileExists = false,
				Filter = "Preset Files (*.ini, *.txt)|*.ini;*.txt",
				DefaultExt = ".ini",
			};

			string filename = Preset.Text ?? string.Empty;
			filename = filename.Split(',')[0];
			if (!string.IsNullOrEmpty(filename))
			{
				dlg.FileName = filename;
				dlg.InitialDirectory = Path.GetDirectoryName(filename);
			}

			if (dlg.ShowDialog(this) == true)
			{
				Preset.Text = dlg.FileName;
			}
		}

		void OnChooseFolderDialog(object sender, RoutedEventArgs e)
		{
			var dlg = new FileOpenDialog
			{
				Multiselect = true,
				FolderPicker = true,
				CheckPathExists = true,
			};

			var target = e.Source as FrameworkElement;

			string directory = target.Tag as string;
			if (!string.IsNullOrEmpty(directory))
			{
				// Get first path in the list
				directory = directory.Split(',')[0].TrimEnd(Path.PathSeparator);
				// Make relative paths absolute (relative to the config file)
				directory = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(configFilePath), directory));
				dlg.InitialDirectory = directory;
			}

			if (dlg.ShowDialog(this) == true)
			{
				target.Tag = string.Join(",", dlg.FileNames);
			}
		}

		private static string CheckboxValue(bool? check) => (check.HasValue && check.Value ? "1" : "0");
	}
}
