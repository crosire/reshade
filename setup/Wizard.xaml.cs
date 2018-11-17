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
using Microsoft.Win32;

namespace ReShade.Setup
{
	public partial class WizardWindow
	{
		bool _isHeadless = false;
		bool _isElevated = false;
		string _configPath = null;
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
			var args = Environment.GetCommandLineArgs().Skip(1).ToArray();

			bool hasApi = false;
			bool hasTargetPath = false;

			for (int i = 0; i < args.Length; i++)
			{
				if (args[i] == "--headless")
				{
					_isHeadless = true;
					continue;
				}
				if (args[i] == "--elevated")
				{
					_isElevated = true;
					continue;
				}

				if (i < args.Length - 1)
				{
					if (args[i] == "--api")
					{
						hasApi = true;

						string api = args[++i];
						ApiDirect3D9.IsChecked = api == "d3d9";
						ApiDirectXGI.IsChecked = api == "dxgi" || api == "d3d10" || api == "d3d11";
						ApiOpenGL.IsChecked = api == "opengl";
						continue;
					}

					if (args[i] == "--top")
					{
						Top = double.Parse(args[++i]);
						continue;
					}
					if (args[i] == "--left")
					{
						Left = double.Parse(args[++i]);
						continue;
					}
				}

				if (File.Exists(args[i]))
				{
					hasTargetPath = true;

					_targetPath = args[i];
				}
			}

			if (hasTargetPath)
			{
				InstallationStep1();

				if (hasApi)
				{
					InstallationStep2();
				}
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
				Assembly assembly = Assembly.GetExecutingAssembly();

				try
				{
					using (FileStream file = File.Create("ReShade32.dll"))
					{
						assembly.GetManifestResourceStream("ReShade.Setup.ReShade32.dll").CopyTo(file);
					}
					using (FileStream file = File.Create("ReShade64.dll"))
					{
						assembly.GetManifestResourceStream("ReShade.Setup.ReShade64.dll").CopyTo(file);
					}
				}
				catch (Exception ex)
				{
					Title = "Failed!";
					Message.Text = "Unable to extract files.";
					MessageDescription.Text = ex.Message;
					SetupButton.IsEnabled = false;
					return;
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

			// Open Steam game installation directory by default if it exists
			if (Directory.Exists(@"C:\Program Files (x86)\Steam\steamapps\common"))
				dlg.InitialDirectory = @"C:\Program Files (x86)\Steam\steamapps\common";

			if (dlg.ShowDialog(this) == true)
			{
				_targetPath = dlg.FileName;

				if (_isElevated || IsWritable(Path.GetDirectoryName(_targetPath)))
				{
					InstallationStep1();
				}
				else
				{
					var startinfo = new ProcessStartInfo {
						Verb = "runas",
						FileName = Assembly.GetExecutingAssembly().Location,
						Arguments = $"\"{_targetPath}\" --elevated --left {Left} --top {Top}"
					};

					Process.Start(startinfo);

					Close();
					return;
				}
			}
		}
		void OnSetupButtonDragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetData(DataFormats.FileDrop, true) is string[] files && files.Length >= 1)
			{
				_targetPath = files[0];

				if (File.Exists(_targetPath))
				{
					if (_isElevated || IsWritable(Path.GetDirectoryName(_targetPath)))
					{
						InstallationStep1();
					}
					else
					{
						var startinfo = new ProcessStartInfo
						{
							Verb = "runas",
							FileName = Assembly.GetExecutingAssembly().Location,
							Arguments = $"\"{_targetPath}\" --elevated --left {Left} --top {Top}"
						};

						Process.Start(startinfo);

						Close();
						return;
					}
				}
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
			Message.Text = "Analyzing " + name + " ...";
			MessageDescription.Visibility = Visibility.Collapsed;

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

			if (isApiD3D8 && !_isHeadless)
			{
				MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'http://reshade.me/d3d8to9' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
			}

			Message.Text = "Select rendering API";
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

			_configPath = Path.ChangeExtension(_targetModulePath, ".ini");

			if (!File.Exists(_configPath))
			{
				_configPath = Path.Combine(Path.GetDirectoryName(_targetPath), "ReShade.ini");
			}

			if (File.Exists(pathModule) && !_isHeadless &&
				MessageBox.Show(this, "Do you want to overwrite the existing installation?", string.Empty, MessageBoxButton.YesNo) != MessageBoxResult.Yes)
			{
				Title += " Failed!";
				Message.Text = "Existing installation found.";
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
			catch (Exception ex)
			{
				Title += " Failed!";
				Message.Text = "Unable to write file \"" + pathModule + "\".";
				MessageDescription.Visibility = Visibility.Visible;
				MessageDescription.Text = ex.Message;
				Glass.HideSystemMenu(this, false);

				if (_isHeadless)
				{
					Environment.Exit(1);
				}
				return;
			}

			Title += " Succeeded!";
			Glass.HideSystemMenu(this, false);

			if (_isHeadless || MessageBox.Show(this, "Do you wish to download a collection of standard effects from https://github.com/crosire/reshade-shaders?", string.Empty, MessageBoxButton.YesNo) == MessageBoxResult.Yes)
			{
				InstallationStep3();
			}
			else
			{
				if (!File.Exists(_configPath))
				{
					string targetDirectory = Path.GetDirectoryName(_targetPath);

					WriteSearchPaths(".\\", ".\\");
				}

				EnableSettingsWindow();
			}
		}
		void InstallationStep3()
		{
			Title = Title.Remove(Title.Length - 11);
			Message.Text = "Downloading ...";
			Glass.HideSystemMenu(this);

			_tempDownloadPath = Path.GetTempFileName();

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) => {
				if (e.Error != null)
				{
					Title += " Failed!";
					Message.Text = "Unable to download archive.";
					MessageDescription.Visibility = Visibility.Visible;
					MessageDescription.Text = e.Error.Message;
					Glass.HideSystemMenu(this, false);

					if (_isHeadless)
						Environment.Exit(1);
				}
				else
				{
					InstallationStep4();
				}
			};

			client.DownloadProgressChanged += (object sender, DownloadProgressChangedEventArgs e) => {
				Message.Text = "Downloading ... (" + ((100 * e.BytesReceived) / e.TotalBytesToReceive) + "%)";
			};

			try
			{
				client.DownloadFileAsync(new Uri("https://github.com/crosire/reshade-shaders/archive/master.zip"), _tempDownloadPath);
			}
			catch (Exception ex)
			{
				Title += " Failed!";
				Message.Text = "Unable to download archive.";
				MessageDescription.Visibility = Visibility.Visible;
				MessageDescription.Text = ex.Message;
				Glass.HideSystemMenu(this, false);

				if (_isHeadless)
					Environment.Exit(1);
			}
		}
		void InstallationStep4()
		{
			Message.Text = "Extracting ...";

			string tempPath = Path.Combine(Path.GetTempPath(), "reshade-shaders-master");
			string tempPathShaders = Path.Combine(tempPath, "Shaders");
			string tempPathTextures = Path.Combine(tempPath, "Textures");
			string targetPath = Path.Combine(Path.GetDirectoryName(_targetPath), "reshade-shaders");
			string targetPathShaders = Path.Combine(targetPath, "Shaders");
			string targetPathTextures = Path.Combine(targetPath, "Textures");

			string[] installedEffects = null;

			if (Directory.Exists(targetPath))
			{
				installedEffects = Directory.GetFiles(targetPathShaders).ToArray();
			}

			try
			{
				if (Directory.Exists(tempPath)) // Delete existing directories since extraction fails if the target is not empty
					Directory.Delete(tempPath, true);

				ZipFile.ExtractToDirectory(_tempDownloadPath, Path.GetTempPath());

				MoveFiles(tempPathShaders, targetPathShaders);
				MoveFiles(tempPathTextures, targetPathTextures);

				File.Delete(_tempDownloadPath);
				Directory.Delete(tempPath, true);
			}
			catch (Exception ex)
			{
				Title += " Failed!";
				Message.Text = "Unable to extract downloaded archive.";
				MessageDescription.Visibility = Visibility.Visible;
				MessageDescription.Text = ex.Message;
				Glass.HideSystemMenu(this, false);

				if (_isHeadless)
					Environment.Exit(1);
				return;
			}

			if (!_isHeadless)
			{
				var wnd = new SelectWindow(Directory.GetFiles(targetPathShaders));
				wnd.Owner = this;

				// If there was an existing installation, select the same effects as previously
				if (installedEffects != null)
				{
					foreach (var item in wnd.GetSelection())
					{
						item.IsChecked = installedEffects.Contains(item.Path);
					}
				}

				wnd.ShowDialog();

				foreach (var item in wnd.GetSelection())
				{
					if (!item.IsChecked)
					{
						try
						{
							File.Delete(item.Path);
						}
						catch
						{
							continue;
						}
					}
				}
			}

			WriteSearchPaths(".\\reshade-shaders\\Shaders", ".\\reshade-shaders\\Textures");

			Title += " Succeeded!";
			Glass.HideSystemMenu(this, false);

			if (_isHeadless)
				Environment.Exit(0);

			EnableSettingsWindow();
		}

		private void MoveFiles(string sourcePath, string targetPath)
		{
			if (!Directory.Exists(targetPath))
			{
				Directory.CreateDirectory(targetPath);
			}

			foreach (string source in Directory.GetFiles(sourcePath))
			{
				string target = targetPath + source.Replace(sourcePath, string.Empty);

				File.Copy(source, target, true);
			}
		}

		private void AddSearchPath(List<string> searchPaths, string newPath)
		{
			string basePath = Path.GetDirectoryName(_targetPath);
			Directory.SetCurrentDirectory(basePath);

			bool pathExists = false;

			foreach (var searchPath in searchPaths)
			{
				if (Path.GetFullPath(searchPath) == newPath)
				{
					pathExists = true;
					break;
				}
			}

			if (!pathExists)
				searchPaths.Add(newPath);
		}
		private void WriteSearchPaths(string targetPathShaders, string targetPathTextures)
		{
			var effectSearchPaths = IniFile.ReadValue(_configPath, "GENERAL", "EffectSearchPaths").Split(',').Where(x => x.Length != 0).ToList();
			var textureSearchPaths = IniFile.ReadValue(_configPath, "GENERAL", "TextureSearchPaths").Split(',').Where(x => x.Length != 0).ToList();

			AddSearchPath(effectSearchPaths, targetPathShaders);
			AddSearchPath(textureSearchPaths, targetPathTextures);

			IniFile.WriteValue(_configPath, "GENERAL", "EffectSearchPaths", string.Join(",", effectSearchPaths));
			IniFile.WriteValue(_configPath, "GENERAL", "TextureSearchPaths", string.Join(",", textureSearchPaths));
		}

		private void EnableSettingsWindow()
		{
			Message.Text = "Edit ReShade settings";
			SetupButton.IsEnabled = true;
			SetupButton.Click -= OnSetupButtonClick;
			SetupButton.Click += (object s, RoutedEventArgs e) => new SettingsWindow(_configPath) { Owner = this }.ShowDialog();
		}
	}
}
