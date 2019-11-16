/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using Microsoft.Win32;
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

namespace ReShade.Setup
{
	public partial class WizardWindow
	{
		bool _is64Bit = false;
		bool _isHeadless = false;
		bool _isElevated = false;
		bool _globalVulkanLayerEnabled = false;
		string _configPath = null;
		string _targetPath = null;
		string _commonPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ReShade");

		public WizardWindow()
		{
			InitializeComponent();
		}

		static void MoveFiles(string sourcePath, string targetPath)
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
		static bool IsWritable(string targetPath)
		{
			try
			{
				File.Create(Path.Combine(targetPath, Path.GetRandomFileName()), 1, FileOptions.DeleteOnClose);
				return true;
			}
			catch
			{
				return false;
			}
		}

		static ZipArchive ExtractArchive()
		{
			var output = new FileStream(Path.GetTempFileName(), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite | FileShare.Delete, 4096, FileOptions.DeleteOnClose);

			using (var input = File.OpenRead(Assembly.GetExecutingAssembly().Location))
			{
				byte[] block = new byte[512];
				byte[] signature = { 0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x00, 0x00 }; // PK..

				// Look for archive at the end of this executable and copy it to a file
				while (input.Read(block, 0, block.Length) >= signature.Length)
				{
					if (block.Take(signature.Length).SequenceEqual(signature))
					{
						output.Write(block, 0, block.Length);
						input.CopyTo(output);
						break;
					}
				}
			}

			return new ZipArchive(output, ZipArchiveMode.Read, false);
		}

		void ShowMessage(string title, string message, string description = null, bool done = false, int exitCode = -1)
		{
			if (done && _isHeadless)
			{
				Environment.Exit(exitCode);
			}
			else if (exitCode == 0)
			{
				message = "Edit ReShade settings";
				SetupButton.IsEnabled = true;
				SetupButton.Click -= OnSetupButtonClick;
				SetupButton.Click += (object s, RoutedEventArgs e) => new SettingsWindow(_configPath) { Owner = this }.ShowDialog();
			}

			Glass.HideSystemMenu(this, !done);

			Title = title;
			Message.Text = message ?? string.Empty;
			MessageDescription.Visibility = string.IsNullOrEmpty(description) ? Visibility.Collapsed : Visibility.Visible;
			MessageDescription.Text = description;
		}

		void RestartAsAdmin()
		{
			Process.Start(new ProcessStartInfo { Verb = "runas", FileName = Assembly.GetExecutingAssembly().Location, Arguments = $"\"{_targetPath}\" --elevated --left {Left} --top {Top}" });

			Close();
		}

		void AddSearchPath(List<string> searchPaths, string newPath)
		{
			string basePath = Path.GetDirectoryName(_targetPath);
			Directory.SetCurrentDirectory(basePath);

			bool pathExists = false;

			foreach (var searchPath in searchPaths)
			{
				if (Path.GetFullPath(searchPath) == Path.GetFullPath(newPath))
				{
					pathExists = true;
					break;
				}
			}

			if (!pathExists)
			{
				searchPaths.Add(newPath);
			}
		}
		void WriteSearchPaths(string targetPathShaders, string targetPathTextures)
		{
			// Vulkan uses a common ReShade DLL for all applications, which is not in the location the shaders and textures are installed to, so make paths absolute
			if (ApiVulkan.IsChecked == true)
			{
				string targetDir = Path.GetDirectoryName(_targetPath);
				targetPathShaders = Path.GetFullPath(Path.Combine(targetDir, targetPathShaders));
				targetPathTextures = Path.GetFullPath(Path.Combine(targetDir, targetPathTextures));
			}

			var iniFile = new IniFile(_configPath);
			List<string> paths = null;

			iniFile.GetValue("GENERAL", "EffectSearchPaths", out var effectSearchPaths);

			paths = new List<string>(effectSearchPaths ?? new string[0]);
			paths.RemoveAll(string.IsNullOrWhiteSpace);
			{
				AddSearchPath(paths, targetPathShaders);
				iniFile.SetValue("GENERAL", "EffectSearchPaths", paths.ToArray());
			}

			iniFile.GetValue("GENERAL", "TextureSearchPaths", out var textureSearchPaths);

			paths = new List<string>(textureSearchPaths ?? new string[0]);
			paths.RemoveAll(string.IsNullOrWhiteSpace);
			{
				AddSearchPath(paths, targetPathTextures);
				iniFile.SetValue("GENERAL", "TextureSearchPaths", paths.ToArray());
			}

			iniFile.Save();
		}

		void InstallationStep0()
		{
			if (!_isElevated && !IsWritable(Path.GetDirectoryName(_targetPath)))
			{
				RestartAsAdmin();
			}
			else
			{
				InstallationStep1();
			}
		}
		void InstallationStep1()
		{
			SetupButton.IsEnabled = false;
			Glass.HideSystemMenu(this);

			var info = FileVersionInfo.GetVersionInfo(_targetPath);
			var name = info.ProductName ?? Path.GetFileNameWithoutExtension(_targetPath);

			ShowMessage("Working on " + name + " ...", "Analyzing " + name + " ...");

			var peInfo = new PEInfo(_targetPath);
			_is64Bit = peInfo.Type == PEInfo.BinaryType.IMAGE_FILE_MACHINE_AMD64;

			var nameModule = peInfo.Modules.FirstOrDefault(s =>
				s.StartsWith("d3d8", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("d3d9", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("dxgi", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("opengl32", StringComparison.OrdinalIgnoreCase) ||
				s.StartsWith("vulkan-1", StringComparison.OrdinalIgnoreCase)) ?? string.Empty;

			bool isApiD3D8 = nameModule.StartsWith("d3d8", StringComparison.OrdinalIgnoreCase);
			bool isApiD3D9 = isApiD3D8 || nameModule.StartsWith("d3d9", StringComparison.OrdinalIgnoreCase);
			bool isApiDXGI = nameModule.StartsWith("dxgi", StringComparison.OrdinalIgnoreCase);
			bool isApiOpenGL = nameModule.StartsWith("opengl32", StringComparison.OrdinalIgnoreCase);
			bool isApiVulkan = nameModule.StartsWith("vulkan-1", StringComparison.OrdinalIgnoreCase);

			if (isApiD3D8 && !_isHeadless)
			{
				MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'http://reshade.me/d3d8to9' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
			}

			Message.Text = "Select the rendering API the game uses:";
			ApiGroup.IsEnabled = true;
			ApiD3D9.IsChecked = isApiD3D9;
			ApiDXGI.IsChecked = isApiDXGI;
			ApiOpenGL.IsChecked = isApiOpenGL;
			ApiVulkan.IsChecked = isApiVulkan;
		}
		void InstallationStep2()
		{
			ApiGroup.IsEnabled = false;

			string targetDir = Path.GetDirectoryName(_targetPath);
			_configPath = Path.Combine(targetDir, "ReShade.ini");

			if (ApiVulkan.IsChecked != true)
			{
				string pathModule = null;
				if (ApiD3D9.IsChecked == true)
				{
					pathModule = "d3d9.dll";
				}
				if (ApiDXGI.IsChecked == true)
				{
					pathModule = "dxgi.dll";
				}
				if (ApiOpenGL.IsChecked == true)
				{
					pathModule = "opengl32.dll";
				}

				pathModule = Path.Combine(targetDir, pathModule);

				var alternativeConfigPath = Path.ChangeExtension(pathModule, ".ini");
				if (File.Exists(alternativeConfigPath))
				{
					_configPath = alternativeConfigPath;
				}

				if (File.Exists(pathModule) && !_isHeadless)
				{
					var result = MessageBox.Show(this, "Do you want to overwrite the existing installation or uninstall ReShade?\n\nPress 'Yes' to overwrite or 'No' to uninstall.", string.Empty, MessageBoxButton.YesNoCancel);

					if (result == MessageBoxResult.No)
					{
						try
						{
							File.Delete(pathModule);

							if (File.Exists(_configPath))
							{
								File.Delete(_configPath);
							}

							if (File.Exists(Path.ChangeExtension(pathModule, ".log")))
							{
								File.Delete(Path.ChangeExtension(pathModule, ".log"));
							}

							if (Directory.Exists(Path.Combine(targetDir, "reshade-shaders")))
							{
								Directory.Delete(Path.Combine(targetDir, "reshade-shaders"), true);
							}
						}
						catch (Exception ex)
						{
							ShowMessage("Failed!", "Unable to delete some files.", ex.Message, true, 1);
							return;
						}

						ShowMessage("Succeeded!", "Successfully uninstalled.", null, true);
						return;
					}
					else if (result != MessageBoxResult.Yes)
					{
						ShowMessage("Failed", "Existing installation found.", null, true, 1);
						return;
					}
				}

				try
				{
					using (ZipArchive zip = ExtractArchive())
					{
						if (zip.Entries.Count != 4)
						{
							throw new FileFormatException("Expected ReShade archive to contain ReShade DLLs");
						}

						// 0: ReShade32.dll
						// 2: ReShade64.dll
						using (Stream input = zip.Entries[_is64Bit ? 2 : 0].Open())
						using (FileStream output = File.Create(pathModule))
						{
							input.CopyTo(output);
						}
					}
				}
				catch (Exception ex)
				{
					ShowMessage("Failed!", "Unable to write file \"" + pathModule + "\".", ex.Message, true, 1);
					return;
				}
			}

			if (_isHeadless || MessageBox.Show(this, "Do you wish to download a collection of standard effects from https://github.com/crosire/reshade-shaders?", string.Empty, MessageBoxButton.YesNo) == MessageBoxResult.Yes)
			{
				InstallationStep3();
			}
			else
			{
				// Add default search paths if no config exists
				if (!File.Exists(_configPath))
				{
					WriteSearchPaths(".\\", ".\\");
				}

				InstallationStep5();
			}
		}
		void InstallationStep3()
		{
			ShowMessage("Downloading ...", "Downloading ...");

			string downloadPath = Path.GetTempFileName();

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) => {
				if (e.Error != null)
				{
					ShowMessage("Failed!", "Unable to download archive.", e.Error.Message, true, 1);
				}
				else
				{
					InstallationStep4(downloadPath);
				}
			};

			client.DownloadProgressChanged += (object sender, DownloadProgressChangedEventArgs e) => {
				Message.Text = "Downloading ... (" + ((100 * e.BytesReceived) / e.TotalBytesToReceive) + "%)";
			};

			try
			{
				client.DownloadFileAsync(new Uri("https://github.com/crosire/reshade-shaders/archive/master.zip"), downloadPath);
			}
			catch (Exception ex)
			{
				ShowMessage("Failed!", "Unable to download archive.", ex.Message);
			}
		}
		void InstallationStep4(string downloadPath)
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
				{
					Directory.Delete(tempPath, true);
				}

				ZipFile.ExtractToDirectory(downloadPath, Path.GetTempPath());

				MoveFiles(tempPathShaders, targetPathShaders);
				MoveFiles(tempPathTextures, targetPathTextures);

				File.Delete(downloadPath);
				Directory.Delete(tempPath, true);
			}
			catch (Exception ex)
			{
				ShowMessage("Failed!", "Unable to extract downloaded archive.", ex.Message, true, 1);
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

			InstallationStep5();
		}
		void InstallationStep5()
		{
			string description = null;
			if (ApiVulkan.IsChecked == true)
			{
				description = "You need to keep the setup tool open for ReShade to work in Vulkan games!\nAlternatively check the option below:";

				ApiGroup.IsEnabled = true;
				ApiD3D9.Visibility = Visibility.Collapsed;
				ApiDXGI.Visibility = Visibility.Collapsed;
				ApiOpenGL.Visibility = Visibility.Collapsed;
				ApiVulkan.Visibility = Visibility.Collapsed;
				ApiVulkanGlobal.IsChecked = _globalVulkanLayerEnabled;
				ApiVulkanGlobal.Visibility = Visibility.Visible;
			}

			ShowMessage("Succeeded!", null, description, true, 0);
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

			// Parse command line arguments
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

				if (i + 1 < args.Length)
				{
					if (args[i] == "--api")
					{
						hasApi = true;

						string api = args[++i];
						ApiD3D9.IsChecked = api == "d3d9";
						ApiDXGI.IsChecked = api == "dxgi" || api == "d3d10" || api == "d3d11";
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
			else
			{
				using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
				{
					_globalVulkanLayerEnabled = key?.GetValue(Path.Combine(_commonPath, Environment.Is64BitOperatingSystem ? "ReShade64.json" : "ReShade32.json")) != null;
				}

				if (!_globalVulkanLayerEnabled)
				{
					InstallVulkanLayer(true);
				}
			}
		}
		void OnWindowClosed(object sender, EventArgs e)
		{
			if (!_globalVulkanLayerEnabled)
			{
				InstallVulkanLayer(false);
			}
		}

		void InstallVulkanLayer(bool enable)
		{
			if (enable)
			{
				if (Directory.Exists(_commonPath))
				{
					Directory.Delete(_commonPath, true);
				}

				Directory.CreateDirectory(_commonPath);

				using (ZipArchive zip = ExtractArchive())
				{
					zip.ExtractToDirectory(_commonPath);
				}
			}
			else
			{
				Directory.Delete(_commonPath, true);
			}

			if (Environment.Is64BitOperatingSystem)
			{
				using (RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\Khronos\Vulkan\ImplicitLayers", true))
				{
					if (enable)
						key.SetValue(Path.Combine(_commonPath, "ReShade64.json"), 0, RegistryValueKind.DWord);
					else
						key.DeleteValue(Path.Combine(_commonPath, "ReShade64.json"));
				}
			}

			using (RegistryKey key = Registry.CurrentUser.CreateSubKey(Environment.Is64BitOperatingSystem ? @"Software\Wow6432Node\Khronos\Vulkan\ImplicitLayers" : @"Software\Khronos\Vulkan\ImplicitLayers", true))
			{
				if (enable)
					key.SetValue(Path.Combine(_commonPath, "ReShade32.json"), 0, RegistryValueKind.DWord);
				else
					key.DeleteValue(Path.Combine(_commonPath, "ReShade32.json"));
			}
		}

		void OnApiChecked(object sender, RoutedEventArgs e)
		{
			InstallationStep2();
		}
		void OnApiVulkanGlobalChecked(object sender, RoutedEventArgs e)
		{
			_globalVulkanLayerEnabled = ((System.Windows.Controls.CheckBox)sender).IsChecked == true;

			InstallVulkanLayer(_globalVulkanLayerEnabled);
		}
		void OnSetupButtonClick(object sender, RoutedEventArgs e)
		{
			if (Keyboard.Modifiers == ModifierKeys.Control)
			{
				try
				{
					using (ZipArchive zip = ExtractArchive())
					{
						zip.ExtractToDirectory(".");
					}
				}
				catch (Exception ex)
				{
					ShowMessage("Failed!", "Unable to extract files.", ex.Message, true);
					SetupButton.IsEnabled = false;
					return;
				}

				Close();
				return;
			}

			var dlg = new OpenFileDialog { Filter = "Applications|*.exe", DefaultExt = ".exe", Multiselect = false, ValidateNames = true, CheckFileExists = true };

			// Open Steam game installation directory by default if it exists
			string steamPath = Path.Combine(
				Environment.GetFolderPath(Environment.Is64BitOperatingSystem ? Environment.SpecialFolder.ProgramFilesX86 : Environment.SpecialFolder.ProgramFiles),
				"Steam", "steamapps", "common");

			if (Directory.Exists(steamPath))
			{
				dlg.InitialDirectory = steamPath;
			}

			if (dlg.ShowDialog(this) == true)
			{
				_targetPath = dlg.FileName;

				InstallationStep0();
			}
		}
		void OnSetupButtonDragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetData(DataFormats.FileDrop, true) is string[] files && files.Length >= 1)
			{
				_targetPath = files[0];

				InstallationStep0();
			}
		}
	}
}
