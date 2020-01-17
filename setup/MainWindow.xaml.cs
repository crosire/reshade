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
	public partial class MainWindow
	{
		bool is64Bit = false;
		bool isHeadless = false;
		bool isElevated = WindowsIdentity.GetCurrent().Owner.IsWellKnown(WellKnownSidType.BuiltinAdministratorsSid);
		bool isFinished = false;
		string configPath = null;
		string targetPath = null;
		string targetName = null;
		string modulePath = null;
		string commonPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "ReShade");

		public MainWindow()
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

			UpdateStatus(success ? "ReShade installation was successful!" : "ReShade installation was not successful!", message, description);

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
			targetName = info.FileDescription;
			if (targetName is null || targetName.Trim().Length == 0)
				targetName = Path.GetFileNameWithoutExtension(targetPath);

			UpdateStatus("Working on " + targetName + " ...", "Analyzing executable ...");

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
				MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'https://github.com/crosire/d3d8to9/releases' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
			}

			Message.Text = "Which rendering API does " + targetName + " use?";

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

			UpdateStatus("Working on " + targetName + " ...", "Installing ReShade ...");

			string targetDir = Path.GetDirectoryName(targetPath);
			configPath = Path.Combine(targetDir, "ReShade.ini");

			if (ApiVulkan.IsChecked != true)
			{
				modulePath = null;
				if (ApiD3D9.IsChecked == true)
				{
					modulePath = "d3d9.dll";
				}
				if (ApiDXGI.IsChecked == true)
				{
					modulePath = "dxgi.dll";
				}
				if (ApiOpenGL.IsChecked == true)
				{
					modulePath = "opengl32.dll";
				}

				if (modulePath == null)
				{
					// No API selected, abort immediately
					return;
				}

				modulePath = Path.Combine(targetDir, modulePath);

				var alternativeConfigPath = Path.ChangeExtension(modulePath, ".ini");
				if (File.Exists(alternativeConfigPath))
				{
					configPath = alternativeConfigPath;
				}

				if (File.Exists(modulePath) && !isHeadless)
				{
					var moduleInfo = FileVersionInfo.GetVersionInfo(modulePath);
					if (moduleInfo.ProductName == "ReShade")
					{
						ApiGroup.Visibility = Visibility.Collapsed;
						InstallButtons.Visibility = Visibility.Visible;

						Message.Text = "Existing ReShade installation found. How do you want to proceed?";
					}
					else
					{
						UpdateStatusAndFinish(false, Path.GetFileName(modulePath) + " already exists, but does not belong to ReShade.", "Please make sure this is not a system file required by the game.");
					}
					return;
				}
			}

			InstallationStep3();
		}
		void InstallationStep3()
		{
			Message.Text = "Installing ReShade ...";

			if (ApiVulkan.IsChecked != true)
			{
				ApiGroup.Visibility = Visibility.Visible;
				InstallButtons.Visibility = Visibility.Collapsed;

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
						using (FileStream output = File.Create(modulePath))
						{
							input.CopyTo(output);
						}
					}
				}
				catch (Exception ex)
				{
					UpdateStatusAndFinish(false, "Unable to write " + Path.GetFileName(modulePath) + ".", ex.Message);
					return;
				}
			}

			// Copy potential pre-made configuration file to target
			if (File.Exists("ReShade.ini") && !File.Exists(configPath))
			{
				try
				{
					File.Copy("ReShade.ini", configPath);
				}
				catch (Exception ex)
				{
					UpdateStatusAndFinish(false, "Unable to write " + Path.GetFileName(configPath) + ".", ex.Message);
					return;
				}
			}

			if (!isHeadless)
			{
				string targetPathShaders = Path.Combine(Path.GetDirectoryName(targetPath), "reshade-shaders", "Shaders");

				string[] installedEffects = null;
				if (Directory.Exists(targetPathShaders))
				{
					installedEffects = Directory.GetFiles(targetPathShaders).Select(x => Path.GetFileName(x)).ToArray();
				}

				var dlg = new SelectEffectsDialog();
				dlg.Owner = this;

				// If there was an existing installation, select the same effects as previously
				if (installedEffects != null)
				{
					foreach (EffectRepositoryItem repository in dlg.EffectList.Items)
					{
						foreach (EffectItem item in repository.Effects)
						{
							item.Enabled = installedEffects.Contains(Path.GetFileName(item.Path));
						}
					}
				}

				if (dlg.ShowDialog() == true)
				{
					var repositoryItems = dlg.EffectList.Items.Cast<EffectRepositoryItem>();
					var repositoryNames = new Queue<string>(repositoryItems.Where(x => x.Enabled != false).Select(x => x.Name));

					if (repositoryNames.Count != 0)
					{
						InstallationStep4(repositoryNames, repositoryItems.SelectMany(x => x.Effects).Where(x => !x.Enabled.Value).Select(x => Path.GetFileName(x.Path)).ToList());
						return;
					}
				}
			}

			// Add default search paths if no config exists
			if (!File.Exists(configPath))
			{
				WriteSearchPaths(".\\", ".\\");
			}

			InstallationStep6();
		}
		void InstallationStep4(Queue<string> repositories, List<string> disabledFiles)
		{
			ApiGroup.IsEnabled = false;
			SetupButton.IsEnabled = false;
			ApiGroup.Visibility = ApiD3D9.Visibility = ApiDXGI.Visibility = ApiOpenGL.Visibility = ApiVulkan.Visibility = Visibility.Visible;
			ApiVulkanGlobal.Visibility = ApiVulkanGlobalButton.Visibility = Visibility.Collapsed;

			string repository = repositories.Dequeue();
			string downloadPath = Path.GetTempFileName();

			UpdateStatus("Working on " + targetName + " ...", "Downloading " + repository + " ...");

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) => {
				if (e.Error != null)
				{
					UpdateStatusAndFinish(false, "Unable to download from " + repository + ".", e.Error.Message);
				}
				else
				{
					InstallationStep5(downloadPath, repository.Substring(repository.IndexOf('/') + 1) + "-master", disabledFiles);

					if (repositories.Count != 0)
					{
						InstallationStep4(repositories, disabledFiles);
					}
				}
			};

			client.DownloadProgressChanged += (object sender, DownloadProgressChangedEventArgs e) => {
				// Avoid negative percentage values
				if (e.TotalBytesToReceive > 0)
				{
					Message.Text = "Downloading " + repository + " ... (" + ((100 * e.BytesReceived) / e.TotalBytesToReceive) + "%)";
				}
			};

			try
			{
				client.DownloadFileAsync(new Uri("https://github.com/" + repository + "/archive/master.zip"), downloadPath);
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Unable to download from " + repository + ".", ex.Message);
			}
		}
		void InstallationStep5(string downloadPath, string downloadName, List<string> disabledFiles)
		{
			UpdateStatus("Working on " + targetName + " ...", "Extracting " + downloadName + " ...");

			string tempPath = Path.Combine(Path.GetTempPath(), downloadName);
			string tempPathShaders = Path.Combine(tempPath, "Shaders");
			string tempPathTextures = Path.Combine(tempPath, "Textures");
			string targetPath = Path.Combine(Path.GetDirectoryName(this.targetPath), "reshade-shaders");
			string targetPathShaders = Path.Combine(targetPath, "Shaders");
			string targetPathTextures = Path.Combine(targetPath, "Textures");

			try
			{
				if (Directory.Exists(tempPath)) // Delete existing directories since extraction fails if the target is not empty
				{
					Directory.Delete(tempPath, true);
				}

				ZipFile.ExtractToDirectory(downloadPath, Path.GetTempPath());

				MoveFiles(tempPathShaders, targetPathShaders);
				if (Directory.Exists(tempPathTextures))
				{
					MoveFiles(tempPathTextures, targetPathTextures);
				}

				File.Delete(downloadPath);
				Directory.Delete(tempPath, true);
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Unable to extract " + downloadName + ".", ex.Message);
				return;
			}

			foreach (var filename in disabledFiles)
			{
				try
				{
					File.Delete(Path.Combine(targetPathShaders, filename));
				}
				catch
				{
					continue;
				}
			}

			WriteSearchPaths(".\\reshade-shaders\\Shaders", ".\\reshade-shaders\\Textures");

			InstallationStep6();
		}
		void InstallationStep6()
		{
			string description = null;
			if (ApiVulkan.IsChecked.Value)
			{
				description = "You need to keep this setup tool open for ReShade to work in Vulkan games!\nAlternatively check the option below:";

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
			else
			{
				description = "You may now close this setup tool or click this button to edit additional settings.";
			}

			UpdateStatusAndFinish(true, "Edit ReShade settings", description);

			SetupButton.IsEnabled = true;
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
					targetName = Path.GetFileNameWithoutExtension(targetPath);
					configPath = Path.Combine(Path.GetDirectoryName(targetPath), "ReShade.ini");
				}
			}

			if (targetPath != null)
			{
				if (hasFinished)
				{
					InstallationStep6();
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
			else
			{
				// Update existing Vulkan layer with the included version
				EnableVulkanLayer(Registry.LocalMachine);
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

		void OnUpdateClick(object sender, RoutedEventArgs e)
		{
			InstallationStep3();
		}
		void OnUninstallClick(object sender, RoutedEventArgs e)
		{
			ApiGroup.Visibility = Visibility.Visible;
			InstallButtons.Visibility = Visibility.Collapsed;

			try
			{
				string targetDir = Path.GetDirectoryName(targetPath);

				File.Delete(modulePath);

				if (File.Exists(configPath))
				{
					File.Delete(configPath);
				}

				if (File.Exists(Path.ChangeExtension(modulePath, ".log")))
				{
					File.Delete(Path.ChangeExtension(modulePath, ".log"));
				}

				if (Directory.Exists(Path.Combine(targetDir, "reshade-shaders")))
				{
					Directory.Delete(Path.Combine(targetDir, "reshade-shaders"), true);
				}

				UpdateStatusAndFinish(true, "Successfully uninstalled.");
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Unable to delete ReShade files.", ex.Message);
			}
		}

		void OnSetupButtonClick(object sender, RoutedEventArgs e)
		{
			if (isFinished)
			{
				new SettingsDialog(configPath) { Owner = this }.ShowDialog();
				return;
			}

			if (targetPath == null && Keyboard.Modifiers == ModifierKeys.Control)
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
					UpdateStatusAndFinish(false, "Unable to extract ReShade files.", ex.Message);
					return;
				}

				Close();
				return;
			}

			var dlg = new SelectAppDialog();
			dlg.Owner = this;

			if (dlg.ShowDialog() == true)
			{
				targetPath = dlg.FileName;

				InstallationStep0();
			}
		}
		void OnSetupButtonDragDrop(object sender, DragEventArgs e)
		{
			if (targetPath != null)
			{
				// Prevent drag and drop if another installation is already in progress
				return;
			}

			if (e.Data.GetData(DataFormats.FileDrop, true) is string[] files && files.Length >= 1)
			{
				targetPath = files[0];

				InstallationStep0();
			}
		}
	}
}
