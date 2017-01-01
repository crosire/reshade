/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using Microsoft.Win32;

namespace ReShade.Setup
{
	public partial class WizardWindow
	{
		bool _isElevated = false;
		string _targetPath = null;
		string _targetModulePath = null;
		PEInfo _targetPEInfo = null;
		string _tempDownloadPath = null;

		public WizardWindow()
		{
			InitializeComponent();
		}

		void OnWindowInit(object sender, EventArgs e)
		{
			Glass.HideIcon(this);
			Glass.ExtendFrame(this);
		}
		void OnWindowLoaded(object sender, RoutedEventArgs e)
		{
			var args = Environment.GetCommandLineArgs();

			if (args.Length > 2 && args[2] == "ELEVATED")
			{
				Top = double.Parse(args[3]);
				Left = double.Parse(args[4]);
				_isElevated = true;
			}
			if (args.Length > 1 && File.Exists(args[1]))
			{
				_targetPath = args[1];
				InstallationStep1();
			}
		}

		void OnApiChecked(object sender, RoutedEventArgs e)
		{
			InstallationStep2();
		}
		void OnSetupButtonClick(object sender, RoutedEventArgs e)
		{
			if (Keyboard.Modifiers == ModifierKeys.Control)
			{
				using (FileStream file = File.Create("ReShade32.dll"))
				{
					Assembly.GetExecutingAssembly().GetManifestResourceStream("ReShade.Setup.ReShade32.dll").CopyTo(file);
				}
				using (FileStream file = File.Create("ReShade64.dll"))
				{
					Assembly.GetExecutingAssembly().GetManifestResourceStream("ReShade.Setup.ReShade64.dll").CopyTo(file);
				}

				Close();
				return;
			}

			var dlg = new OpenFileDialog {
				Filter = "Applications|*.exe",
				DefaultExt = ".exe",
				Multiselect = false,
				ValidateNames = true,
				CheckFileExists = true
			};

			if (dlg.ShowDialog(this) == true)
			{
				if (_isElevated || IsWritable(Path.GetDirectoryName(dlg.FileName)))
				{
					_targetPath = dlg.FileName;
					InstallationStep1();
				}
				else
				{
					var startinfo = new ProcessStartInfo {
						Verb = "runas",
						FileName = Assembly.GetExecutingAssembly().Location,
						Arguments = "\"" + dlg.FileName + "\" ELEVATED " + Left + " " + Top
					};

					Process.Start(startinfo);

					Close();
					return;
				}
			}
		}
		void OnSetupButtonDragDrop(object sender, DragEventArgs e)
		{
			if (!e.Data.GetDataPresent(DataFormats.FileDrop))
			{
				return;
			}

			var files = e.Data.GetData(DataFormats.FileDrop, true) as string[];

			if (files != null)
			{
				_targetPath = files[0];
				InstallationStep1();
			}
		}

		static bool IsWritable(string destDirName)
		{
			try
			{
				File.Create(Path.Combine(destDirName, Path.GetRandomFileName()), 1, FileOptions.DeleteOnClose);
				return true;
			}
			catch
			{
				return false;
			}
		}

		void InstallationStep1()
		{
			SetupButton.IsEnabled = false;
			Glass.HideSystemMenu(this);

			var info = FileVersionInfo.GetVersionInfo(_targetPath);
			string name = !string.IsNullOrEmpty(info.ProductName) ? info.ProductName : Path.GetFileNameWithoutExtension(_targetPath);
			_targetPEInfo = new PEInfo(_targetPath);

			Title = "Installing to " + name + " ...";
			Message.Content = "Analyzing " + name + " ...";

			string nameModule = _targetPEInfo.Modules.FirstOrDefault(s =>
				s.StartsWith("d3d8", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("d3d9", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("dxgi", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("opengl32", StringComparison.OrdinalIgnoreCase));

			if (nameModule == null)
			{
				nameModule = string.Empty;
			}

			bool isApiD3D8 = nameModule.StartsWith("d3d8", StringComparison.OrdinalIgnoreCase);
			bool isApiD3D9 = isApiD3D8 || nameModule.StartsWith("d3d9", StringComparison.OrdinalIgnoreCase);
			bool isApiDXGI = nameModule.StartsWith("dxgi", StringComparison.OrdinalIgnoreCase);
			bool isApiOpenGL = nameModule.StartsWith("opengl32", StringComparison.OrdinalIgnoreCase);

			if (isApiD3D8)
			{
				MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'http://reshade.me/d3d8to9' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
			}

			Message.Content = "Select rendering API";
			ApiGroup.IsEnabled = true;
			ApiDirect3D9.IsChecked = isApiD3D9;
			ApiDirectXGI.IsChecked = isApiDXGI;
			ApiOpenGL.IsChecked = isApiOpenGL;
		}
		void InstallationStep2()
		{
			string nameModule = null;
			ApiGroup.IsEnabled = false;
			if (ApiDirect3D9.IsChecked == true)
				nameModule = "d3d9.dll";
			if (ApiDirectXGI.IsChecked == true)
				nameModule = "dxgi.dll";
			if (ApiOpenGL.IsChecked == true)
				nameModule = "opengl32.dll";

			string pathModule = _targetModulePath = Path.Combine(Path.GetDirectoryName(_targetPath), nameModule);

			if (File.Exists(pathModule) &&
				MessageBox.Show(this, "Do you want to overwrite the existing installation?", string.Empty, MessageBoxButton.YesNo) != MessageBoxResult.Yes)
			{
				Title += " Failed!";
				Message.Content = "Existing installation found.";
				Glass.HideSystemMenu(this, false);
				return;
			}

			try
			{
				Stream stream = _targetPEInfo.Type == PEInfo.BinaryType.IMAGE_FILE_MACHINE_AMD64
					? Assembly.GetExecutingAssembly().GetManifestResourceStream("ReShade.Setup.ReShade64.dll")
					: Assembly.GetExecutingAssembly().GetManifestResourceStream("ReShade.Setup.ReShade32.dll");

				using (FileStream file = File.Create(pathModule))
				{
					stream.CopyTo(file);
				}
			}
			catch
			{
				Title += " Failed!";
				Message.Content = "Unable to write file \"" + pathModule + "\".";
				Glass.HideSystemMenu(this, false);
				return;
			}

			Title += " Succeeded!";
			Message.Content = "Done";
			Glass.HideSystemMenu(this, false);

			if (MessageBox.Show(this, "Do you wish to download a collection of standard effects from https://github.com/crosire/reshade-shaders?", string.Empty, MessageBoxButton.YesNo) == MessageBoxResult.Yes)
			{
				InstallationStep3();
			}
			else
			{
				string configFilePath = Path.ChangeExtension(_targetModulePath, ".ini");

				if (!File.Exists(configFilePath))
				{
					string targetDirectory = Path.GetDirectoryName(_targetPath);

					IniFile.WriteValue(configFilePath, "GENERAL", "EffectSearchPaths", targetDirectory);
					IniFile.WriteValue(configFilePath, "GENERAL", "TextureSearchPaths", targetDirectory);
				}
			}
		}
		void InstallationStep3()
		{
			Title = Title.Remove(Title.Length - 11);
			Message.Content = "Downloading ...";
			Glass.HideSystemMenu(this);

			_tempDownloadPath = Path.GetTempFileName();

			var client = new WebClient();
			client.DownloadProgressChanged += (object sender, DownloadProgressChangedEventArgs e)
				=> {
					Message.Content = "Downloading ... (" + ((100 * e.BytesReceived) / e.TotalBytesToReceive) + "%)";
				};
			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e)
				=> {
					if (e.Error != null || e.Cancelled)
					{
						Title += " Failed!";
						Message.Content = "Unable to download archive.";
						Glass.HideSystemMenu(this, false);
					}
					else
					{
						InstallationStep4();
					}
				};

			try
			{
				client.DownloadFileAsync(new Uri("https://github.com/crosire/reshade-shaders/archive/master.zip"), _tempDownloadPath);
			}
			catch
			{
				Title += " Failed!";
				Message.Content = "Unable to download archive.";
				Glass.HideSystemMenu(this, false);
			}
		}
		void InstallationStep4()
		{
			string targetDirectory = Path.GetDirectoryName(_targetPath);
			string shadersDirectoryFinal = Path.Combine(targetDirectory, "reshade-shaders");
			string shadersDirectoryExtracted = Path.Combine(targetDirectory, "reshade-shaders-master");

			try
			{
				if (Directory.Exists(shadersDirectoryFinal))
				{
					Directory.Delete(shadersDirectoryFinal, true);
				}
				if (Directory.Exists(shadersDirectoryExtracted))
				{
					Directory.Delete(shadersDirectoryExtracted, true);
				}

				ZipFile.ExtractToDirectory(_tempDownloadPath, targetDirectory);
				File.Delete(_tempDownloadPath);

				Directory.Move(shadersDirectoryExtracted, shadersDirectoryFinal);
			}
			catch
			{
				Title += " Failed!";
				Message.Content = "Unable to extract downloaded archive.";
				Glass.HideSystemMenu(this, false);
				return;
			}

			var wnd = new SelectWindow(Directory.GetFiles(Path.Combine(shadersDirectoryFinal, "Shaders")));
			wnd.Owner = this;
			wnd.ShowDialog();

			foreach (var item in wnd.GetSelection())
			{
				if (item.IsChecked)
					continue;

				File.Delete(item.Path);
			}

			string configFilePath = Path.ChangeExtension(_targetModulePath, ".ini");

			var effectSearchPaths = new HashSet<string>(IniFile.ReadValue(configFilePath, "GENERAL", "EffectSearchPaths", targetDirectory).Split(','));
			var textureSearchPaths = new HashSet<string>(IniFile.ReadValue(configFilePath, "GENERAL", "TextureSearchPaths", targetDirectory).Split(','));
			effectSearchPaths.Add(Path.Combine(shadersDirectoryFinal, "Shaders"));
			textureSearchPaths.Add(Path.Combine(shadersDirectoryFinal, "Textures"));
			IniFile.WriteValue(configFilePath, "GENERAL", "EffectSearchPaths", string.Join(",", effectSearchPaths));
			IniFile.WriteValue(configFilePath, "GENERAL", "TextureSearchPaths", string.Join(",", textureSearchPaths));

			Title += " Succeeded!";
			Message.Content = "Done";
			Glass.HideSystemMenu(this, false);
		}
	}
}
