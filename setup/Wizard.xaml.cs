/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using Microsoft.Win32;
using ReShade.Utilities;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Principal;
using System.Windows;
using System.Windows.Input;

namespace ReShade.Setup
{
	public partial class WizardWindow
	{
		bool is64Bit = false;
		bool isHeadless = false;
		bool isElevated = WindowsIdentity.GetCurrent().Owner.IsWellKnown(WellKnownSidType.BuiltinAdministratorsSid);
		bool isFinished = false;
		string configPath = null;
		string targetPath = null;
		string commonPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "ReShade");

		public WizardWindow()
		{
			InitializeComponent();

			ApiVulkanGlobal.IsChecked = IsVulkanLayerEnabled(Registry.LocalMachine);

			if (isElevated)
			{
				ApiGroup.Visibility = Visibility.Visible;
				ApiVulkanGlobalButton.Visibility = Visibility.Collapsed;
			}
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

		void AddSearchPath(List<string> searchPaths, string newPath)
		{
			string basePath = Path.GetDirectoryName(ApiVulkan.IsChecked.Value ? commonPath : targetPath);
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
			if (ApiVulkan.IsChecked.Value)
			{
				string targetDir = Path.GetDirectoryName(targetPath);
				targetPathShaders = Path.GetFullPath(Path.Combine(targetDir, targetPathShaders));
				targetPathTextures = Path.GetFullPath(Path.Combine(targetDir, targetPathTextures));
			}

			var iniFile = new IniFile(configPath);
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

		bool EnableVulkanLayer(RegistryKey hive)
		{
			try
			{
				if (Directory.Exists(commonPath))
				{
					Directory.Delete(commonPath, true);
				}

				Directory.CreateDirectory(commonPath);

				using (ZipArchive zip = ExtractArchive())
				{
					zip.ExtractToDirectory(commonPath);
				}

				if (Environment.Is64BitOperatingSystem)
				{
					using (RegistryKey key = hive.CreateSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
					{
						key.SetValue(Path.Combine(commonPath, "ReShade64.json"), 0, RegistryValueKind.DWord);
					}
				}

				using (RegistryKey key = hive.CreateSubKey(Environment.Is64BitOperatingSystem ? @"Software\Wow6432Node\Khronos\Vulkan\ImplicitLayers" : @"Software\Khronos\Vulkan\ImplicitLayers"))
				{
					key.SetValue(Path.Combine(commonPath, "ReShade32.json"), 0, RegistryValueKind.DWord);
				}

				return true;
			}
			catch
			{
				return false;
			}
		}
		bool DisableVulkanLayer(RegistryKey hive)
		{
			try
			{
				if (Directory.Exists(commonPath))
				{
					Directory.Delete(commonPath, true);
				}

				if (Environment.Is64BitOperatingSystem)
				{
					using (RegistryKey key = hive.CreateSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
					{
						key.DeleteValue(Path.Combine(commonPath, "ReShade64.json"));
					}
				}

				using (RegistryKey key = hive.CreateSubKey(Environment.Is64BitOperatingSystem ? @"Software\Wow6432Node\Khronos\Vulkan\ImplicitLayers" : @"Software\Khronos\Vulkan\ImplicitLayers"))
				{
					key.DeleteValue(Path.Combine(commonPath, "ReShade32.json"));
				}

				return true;
			}
			catch
			{
				return false;
			}
		}
		bool IsVulkanLayerEnabled(RegistryKey hive)
		{
			using (RegistryKey key = hive.OpenSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
			{
				return key?.GetValue(Path.Combine(commonPath, Environment.Is64BitOperatingSystem ? "ReShade64.json" : "ReShade32.json")) != null;
			}
		}

		void UpdateStatus(string title, string message, string description = null)
		{
			Title = title;
			Message.Text = message ?? string.Empty;
			MessageDescription.Visibility = string.IsNullOrEmpty(description) ? Visibility.Collapsed : Visibility.Visible;
			MessageDescription.Text = description;

			Glass.HideSystemMenu(this);
		}
		void UpdateStatusAndFinish(bool success, string message, string description = null)
		{
			isFinished = true;
			SetupButton.IsEnabled = false; // Use button as text box only

			UpdateStatus(success ? "Succeeded!" : "Failed!", message, description);

			Glass.HideSystemMenu(this, false);

			if (isHeadless)
			{
				Environment.Exit(success ? 0 : 1);
			}
		}

		bool RestartWithElevatedPrivileges()
		{
			var startInfo = new ProcessStartInfo {
				Verb = "runas",
				FileName = Assembly.GetExecutingAssembly().Location,
				Arguments = $"\"{targetPath}\" --elevated --left {Left} --top {Top}"
			};

			if (ApiD3D9.IsChecked.Value)
				startInfo.Arguments += " --api d3d9";
			if (ApiDXGI.IsChecked.Value)
				startInfo.Arguments += " --api dxgi";
			if (ApiOpenGL.IsChecked.Value)
				startInfo.Arguments += " --api opengl";
			if (ApiVulkan.IsChecked.Value)
				startInfo.Arguments += " --api vulkan";

			if (isFinished)
				startInfo.Arguments += " --finished";

			try
			{
				Process.Start(startInfo);
				Close();
				return true;
			}
			catch
			{
				return false;
			}
		}

		void InstallationStep0()
		{
			if (!isElevated && !IsWritable(Path.GetDirectoryName(targetPath)))
			{
				RestartWithElevatedPrivileges();
			}
			else
			{
				InstallationStep1();
			}
		}
		void InstallationStep1()
		{
			ApiGroup.IsEnabled = true;
			SetupButton.IsEnabled = false;
			ApiGroup.Visibility = ApiD3D9.Visibility = ApiDXGI.Visibility = ApiOpenGL.Visibility = ApiVulkan.Visibility = Visibility.Visible;
			ApiVulkanGlobal.Visibility = ApiVulkanGlobalButton.Visibility = Visibility.Collapsed;

			var info = FileVersionInfo.GetVersionInfo(targetPath);
			var name = info.ProductName ?? Path.GetFileNameWithoutExtension(targetPath);

			UpdateStatus("Working on " + name + " ...", "Analyzing " + name + " ...");

			var peInfo = new PEInfo(targetPath);
			is64Bit = peInfo.Type == PEInfo.BinaryType.IMAGE_FILE_MACHINE_AMD64;

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

			if (isApiD3D8 && !isHeadless)
			{
				MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'http://reshade.me/d3d8to9' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
			}

			Message.Text = "Select the rendering API the game uses:";

			ApiD3D9.IsChecked = isApiD3D9;
			ApiDXGI.IsChecked = isApiDXGI;
			ApiOpenGL.IsChecked = isApiOpenGL;
			ApiVulkan.IsChecked = isApiVulkan;
		}
		void InstallationStep2()
		{
			ApiGroup.IsEnabled = false;
			SetupButton.IsEnabled = false;
			ApiGroup.Visibility = ApiD3D9.Visibility = ApiDXGI.Visibility = ApiOpenGL.Visibility = ApiVulkan.Visibility = Visibility.Visible;
			ApiVulkanGlobal.Visibility = ApiVulkanGlobalButton.Visibility = Visibility.Collapsed;

			string targetDir = Path.GetDirectoryName(targetPath);
			configPath = Path.Combine(targetDir, "ReShade.ini");

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

				if (pathModule == null)
				{
					// No API selected, abort immediately
					return;
				}

				pathModule = Path.Combine(targetDir, pathModule);

				var alternativeConfigPath = Path.ChangeExtension(pathModule, ".ini");
				if (File.Exists(alternativeConfigPath))
				{
					configPath = alternativeConfigPath;
				}

				if (File.Exists(pathModule) && !isHeadless)
				{
					var result = MessageBox.Show(this, "Do you want to overwrite the existing installation or uninstall ReShade?\n\nPress 'Yes' to overwrite or 'No' to uninstall.", string.Empty, MessageBoxButton.YesNoCancel);

					if (result == MessageBoxResult.No)
					{
						try
						{
							File.Delete(pathModule);

							if (File.Exists(configPath))
							{
								File.Delete(configPath);
							}

							if (File.Exists(Path.ChangeExtension(pathModule, ".log")))
							{
								File.Delete(Path.ChangeExtension(pathModule, ".log"));
							}

							if (Directory.Exists(Path.Combine(targetDir, "reshade-shaders")))
							{
								Directory.Delete(Path.Combine(targetDir, "reshade-shaders"), true);
							}

							UpdateStatusAndFinish(true, "Successfully uninstalled.");
						}
						catch (Exception ex)
						{
							UpdateStatusAndFinish(false, "Unable to delete some files.", ex.Message);
						}
						return;
					}
					else if (result != MessageBoxResult.Yes)
					{
						UpdateStatusAndFinish(false, "Existing installation found.");
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
						using (Stream input = zip.Entries[is64Bit ? 2 : 0].Open())
						using (FileStream output = File.Create(pathModule))
						{
							input.CopyTo(output);
						}
					}
				}
				catch (Exception ex)
				{
					UpdateStatusAndFinish(false, "Unable to write file \"" + pathModule + "\".", ex.Message);
					return;
				}
			}

			if (isHeadless || MessageBox.Show(this, "Do you wish to download a collection of standard effects from https://github.com/crosire/reshade-shaders?", string.Empty, MessageBoxButton.YesNo) == MessageBoxResult.Yes)
			{
				InstallationStep3();
			}
			else
			{
				// Add default search paths if no config exists
				if (!File.Exists(configPath))
				{
					WriteSearchPaths(".\\", ".\\");
				}

				InstallationStep5();
			}
		}
		void InstallationStep3()
		{
			UpdateStatus("Downloading ...", "Downloading ...");

			string downloadPath = Path.GetTempFileName();

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) => {
				if (e.Error != null)
				{
					UpdateStatusAndFinish(false, "Unable to download archive.", e.Error.Message);
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
				UpdateStatus("Failed!", "Unable to download archive.", ex.Message);
			}
		}
		void InstallationStep4(string downloadPath)
		{
			Message.Text = "Extracting ...";

			string tempPath = Path.Combine(Path.GetTempPath(), "reshade-shaders-master");
			string tempPathShaders = Path.Combine(tempPath, "Shaders");
			string tempPathTextures = Path.Combine(tempPath, "Textures");
			string targetPath = Path.Combine(Path.GetDirectoryName(this.targetPath), "reshade-shaders");
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
				UpdateStatusAndFinish(false, "Unable to extract downloaded archive.", ex.Message);
				return;
			}

			if (!isHeadless)
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
			if (ApiVulkan.IsChecked.Value)
			{
				description = "You need to keep the setup tool open for ReShade to work in Vulkan games!\nAlternatively check the option below:";

				ApiGroup.IsEnabled = true;
				ApiD3D9.Visibility = ApiDXGI.Visibility = ApiOpenGL.Visibility = ApiVulkan.Visibility = Visibility.Collapsed;
				ApiVulkanGlobal.Visibility = Visibility.Visible;

				if (isElevated)
				{
					ApiGroup.Visibility = Visibility.Visible;
					ApiVulkanGlobalButton.Visibility = Visibility.Collapsed;
				}
				else
				{
					ApiGroup.Visibility = Visibility.Collapsed;
					ApiVulkanGlobalButton.Visibility = Visibility.Visible;
				}
			}

			UpdateStatusAndFinish(true, "Edit ReShade settings", description);

			SetupButton.IsEnabled = true;
			SetupButton.Click -= OnSetupButtonClick;
			SetupButton.Click += (object s, RoutedEventArgs e) => new SettingsWindow(configPath) { Owner = this }.ShowDialog();
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
			bool hasFinished = false;

			// Parse command line arguments
			for (int i = 0; i < args.Length; i++)
			{
				if (args[i] == "--headless")
				{
					isHeadless = true;
					continue;
				}
				if (args[i] == "--elevated")
				{
					isElevated = true;
					continue;
				}
				if (args[i] == "--finished")
				{
					hasFinished = true;
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
						ApiVulkan.IsChecked = api == "vulkan";
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
					targetPath = args[i];
					configPath = Path.Combine(Path.GetDirectoryName(targetPath), "ReShade.ini");
				}
			}

			if (targetPath != null)
			{
				if (hasFinished)
				{
					InstallationStep5();
				}
				else if (hasApi)
				{
					InstallationStep2();
				}
				else
				{
					InstallationStep1();
				}
				return;
			}

			if (!ApiVulkanGlobal.IsChecked.Value)
			{
				// Enable Vulkan layer while the setup tool is running
				EnableVulkanLayer(Registry.CurrentUser);
			}
		}
		void OnWindowClosed(object sender, EventArgs e)
		{
			if (!ApiVulkanGlobal.IsChecked.Value)
			{
				// Disable Vulkan layer again when the setup tool is being closed
				DisableVulkanLayer(Registry.CurrentUser);
			}
		}

		void OnApiChecked(object sender, RoutedEventArgs e)
		{
			InstallationStep2();
		}
		void OnApiVulkanGlobalChecked(object sender, RoutedEventArgs e)
		{
			if (sender is System.Windows.Controls.Button button)
			{
				RestartWithElevatedPrivileges();
				return;
			}

			var checkbox = sender as System.Windows.Controls.CheckBox;
			if (checkbox.IsChecked == IsVulkanLayerEnabled(Registry.LocalMachine))
			{
				return;
			}

			// Should have elevated privileges at this point
			if (!isElevated)
			{
				// Reset check box to previous value if unable to get elevated privileges
				checkbox.IsChecked = !checkbox.IsChecked;
				return;
			}

			// Switch between installing to HKLM and HKCU based on check box value
			if (checkbox.IsChecked.Value)
			{
				DisableVulkanLayer(Registry.CurrentUser);
				if (!EnableVulkanLayer(Registry.LocalMachine))
				{
					UpdateStatusAndFinish(false, "Failed to install global Vulkan layer.");
					checkbox.IsChecked = !checkbox.IsChecked;
				}
			}
			else
			{
				DisableVulkanLayer(Registry.LocalMachine);
				if (!EnableVulkanLayer(Registry.CurrentUser))
				{
					UpdateStatusAndFinish(false, "Failed to install user local Vulkan layer.");
					checkbox.IsChecked = !checkbox.IsChecked;
				}
			}
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
					UpdateStatusAndFinish(false, "Unable to extract files.", ex.Message);
					return;
				}

				Close();
				return;
			}

			var dlg = new OpenFileDialog
			{
				Filter = "Applications|*.exe",
				DefaultExt = ".exe",
				Multiselect = false,
				ValidateNames = true,
				CheckFileExists = true
			};

			// Open Steam game installation directory by default if it exists
			string steamPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "Steam", "steamapps", "common");
			if (Directory.Exists(steamPath))
			{
				dlg.InitialDirectory = steamPath;
			}

			if (dlg.ShowDialog(this) == true)
			{
				targetPath = dlg.FileName;

				InstallationStep0();
			}
		}
		void OnSetupButtonDragDrop(object sender, DragEventArgs e)
		{
			if (e.Data.GetData(DataFormats.FileDrop, true) is string[] files && files.Length >= 1)
			{
				targetPath = files[0];

				InstallationStep0();
			}
		}
	}
}
