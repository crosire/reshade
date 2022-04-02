/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Principal;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Media;
using System.Windows.Navigation;
using Microsoft.Win32;
using ReShade.Setup.Pages;
using ReShade.Setup.Utilities;

namespace ReShade.Setup
{
	public partial class MainWindow
	{
		bool isHeadless = false;
		bool isElevated = WindowsIdentity.GetCurrent().Owner.IsWellKnown(WellKnownSidType.BuiltinAdministratorsSid);
		bool isFinished = false;

		ZipArchive zip;
		IniFile packagesIni;
		IniFile compatibilityIni;

		StatusPage status = new StatusPage();
		SelectAppPage appPage = new SelectAppPage();

		Api targetApi = Api.Unknown;
		bool is64Bit;
		bool isUpdate;
		string targetPath;
		string targetName;
		string configPath;
		string modulePath;
		string presetPath;
		static readonly string commonPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "ReShade");
		string tempPath;
		string tempPathEffects;
		string tempPathTextures;
		string targetPathEffects;
		string targetPathTextures;
		string downloadPath;
		Queue<EffectPackage> packages;
		string[] effects;
		EffectPackage package;

		public MainWindow()
		{
			InitializeComponent();

			var assembly = Assembly.GetExecutingAssembly();
			var productVersion = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>().InformationalVersion;
			Title = "ReShade Setup v" + productVersion;

			try
			{
				// Extract archive attached to this executable
				var output = new FileStream(Path.GetTempFileName(), FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.ReadWrite | FileShare.Delete, 4096, FileOptions.DeleteOnClose);

				using (var input = File.OpenRead(assembly.Location))
				{
					byte[] block = new byte[512];
					byte[] signature = { 0x50, 0x4B, 0x03, 0x04 }; // PK..

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

				zip = new ZipArchive(output, ZipArchiveMode.Read, false);

				// Validate archive contains the ReShade DLLs
				if (zip.GetEntry("ReShade32.dll") == null || zip.GetEntry("ReShade64.dll") == null)
				{
					throw new InvalidDataException();
				}
			}
			catch (Exception)
			{
				MessageBox.Show("This setup archive is corrupted! Please download from https://reshade.me again.");
				Environment.Exit(1);
				return;
			}

			var signed = false;
			if (productVersion.Contains(" "))
			{
				NavigationPanel.Background = Brushes.Crimson;
			}
			else
			{
				signed = assembly.GetCustomAttribute<AssemblyConfigurationAttribute>().Configuration.Contains("Signed");

				if (!signed)
				{
					NavigationPanel.Background = new SolidColorBrush(Color.FromArgb(255, 237, 189, 0));
				}
			}

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			// Attempt to download effect package and compatibility list
			using (var client = new WebClient())
			{
				try
				{
					using (var packagesStream = client.OpenRead("https://raw.githubusercontent.com/crosire/reshade-shaders/list/EffectPackages.ini"))
						packagesIni = new IniFile(packagesStream);
					using (var compatibilityStream = client.OpenRead("https://raw.githubusercontent.com/crosire/reshade-shaders/list/Compatibility.ini"))
						compatibilityIni = new IniFile(compatibilityStream);
				}
				catch
				{
					// Ignore if these lists fail to download, since setup can still proceed without them
				}
			}

			var args = Environment.GetCommandLineArgs().Skip(1).ToArray();

			// Parse command line arguments
			for (int i = 0; i < args.Length; i++)
			{
				if (args[i] == "--headless")
				{
					Visibility = Visibility.Hidden;

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
					isFinished = true;
					continue;
				}

				if (i + 1 < args.Length)
				{
					if (args[i] == "--api")
					{
						string api = args[++i];
						if (api == "d3d9")
						{
							targetApi = Api.D3D9;
						}
						else if (api == "dxgi" || api == "d3d10" || api == "d3d11")
						{
							targetApi = Api.DXGI;
						}
						else if (api == "opengl")
						{
							targetApi = Api.OpenGL;
						}
						else if (api == "vulkan")
						{
							targetApi = Api.Vulkan;
						}
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
				}
			}

			if (targetPath != null)
			{
				if (isFinished)
				{
					InstallStep8();
				}
				else if (targetApi != Api.Unknown)
				{
					var peInfo = new PEInfo(targetPath);
					is64Bit = peInfo.Type == PEInfo.BinaryType.IMAGE_FILE_MACHINE_AMD64;

					InstallStep2();
				}
				else
				{
					InstallStep1();
				}
			}
			else if (isHeadless)
			{
				UpdateStatusAndFinish(false, "No target application was provided.");
			}
			else
			{
				NextButton.IsEnabled = false;

				appPage.PathBox.TextChanged += (sender2, e2) => NextButton.IsEnabled = !string.IsNullOrEmpty(appPage.FileName) && Path.GetExtension(appPage.FileName).Equals(".exe", StringComparison.OrdinalIgnoreCase) && File.Exists(appPage.FileName);

				ResetStatus();

				if (!signed)
				{
					MessageBox.Show("This build of ReShade is intended for singleplayer games only and may cause bans in multiplayer games.", "Warning", MessageBoxButton.OK, MessageBoxImage.Exclamation);
				}
			}
		}

		static void MoveFiles(string sourcePath, string targetPath)
		{
			if (Directory.Exists(targetPath) == false)
			{
				Directory.CreateDirectory(targetPath);
			}

			foreach (string source in Directory.GetFiles(sourcePath))
			{
				string target = targetPath + source.Replace(sourcePath, string.Empty);

				File.Copy(source, target, true);
			}

			// Copy files recursively
			foreach (string source in Directory.GetDirectories(sourcePath))
			{
				string target = targetPath + source.Replace(sourcePath, string.Empty);

				MoveFiles(source, target);
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

		static bool ModuleExists(string path, out bool isReShade)
		{
			if (File.Exists(path))
			{
				isReShade = FileVersionInfo.GetVersionInfo(path).ProductName == "ReShade";
				return true;
			}
			else
			{
				isReShade = false;
				return false;
			}
		}

		static void RunTaskWithExceptionHandling(Action action)
		{
			Task.Run(action).ContinueWith(c => Environment.FailFast("Unhandled exception during installation", c.Exception), TaskContinuationOptions.OnlyOnFaulted | TaskContinuationOptions.ExecuteSynchronously);
		}

		void AddSearchPath(List<string> searchPaths, string newPath)
		{
			if (searchPaths.Any(searchPath => Path.GetFullPath(searchPath) == Path.GetFullPath(newPath)))
			{
				return;
			}

			searchPaths.Add(newPath);
		}
		void WriteSearchPaths(string targetPathEffects, string targetPathTextures)
		{
			// Change current directory so that "Path.GetFullPath" resolves correctly
			string currentPath = Directory.GetCurrentDirectory();
			Directory.SetCurrentDirectory(Path.GetDirectoryName(configPath));

			// Vulkan uses a common ReShade DLL for all applications, which is not in the location the effects and textures are installed to, so make paths absolute
			if (targetApi == Api.Vulkan)
			{
				string targetDir = Path.GetDirectoryName(targetPath);
				targetPathEffects = Path.GetFullPath(Path.Combine(targetDir, targetPathEffects));
				targetPathTextures = Path.GetFullPath(Path.Combine(targetDir, targetPathTextures));
			}

			var iniFile = new IniFile(configPath);
			List<string> paths = null;

			iniFile.GetValue("GENERAL", "EffectSearchPaths", out var effectSearchPaths);

			paths = new List<string>(effectSearchPaths ?? new string[0]);
			paths.RemoveAll(string.IsNullOrWhiteSpace);
			{
				AddSearchPath(paths, targetPathEffects);
				iniFile.SetValue("GENERAL", "EffectSearchPaths", paths.ToArray());
			}

			iniFile.GetValue("GENERAL", "TextureSearchPaths", out var textureSearchPaths);

			paths = new List<string>(textureSearchPaths ?? new string[0]);
			paths.RemoveAll(string.IsNullOrWhiteSpace);
			{
				AddSearchPath(paths, targetPathTextures);
				iniFile.SetValue("GENERAL", "TextureSearchPaths", paths.ToArray());
			}

			iniFile.SaveFile();

			Directory.SetCurrentDirectory(currentPath);
		}

		void ResetStatus()
		{
			isUpdate = false;
			isFinished = false;

			targetApi = Api.Unknown;
			targetPath = targetName = configPath = modulePath = presetPath = tempPath = tempPathEffects = tempPathTextures = targetPathEffects = targetPathTextures = downloadPath = null;
			packages = null; effects = null; package = null;

			Dispatcher.Invoke(() =>
			{
				CurrentPage.Navigate(appPage);

				int statusTitleIndex = Title.IndexOf(" was");
				if (statusTitleIndex > 0)
				{
					Title = Title.Remove(statusTitleIndex);
				}
			});
		}
		void UpdateStatus(string message)
		{
			Dispatcher.Invoke(() =>
			{
				status.UpdateStatus(message);

				if (CurrentPage.Content != status)
				{
					CurrentPage.Navigate(status);
				}

				AeroGlass.HideSystemMenu(this, true);
			});

			if (isHeadless)
			{
				Console.WriteLine(message);
			}
		}
		void UpdateStatusAndFinish(bool success, string message)
		{
			isFinished = true;

			Dispatcher.Invoke(() =>
			{
				status.UpdateStatus(message, success);

				CurrentPage.Navigate(status);

				Title += success ? " was successful!" : " was not successful!";

				AeroGlass.HideSystemMenu(this, false);
			});

			if (isHeadless)
			{
				Console.WriteLine(message);

				Environment.Exit(success ? 0 : 1);
			}
		}

		bool RestartWithElevatedPrivileges()
		{
			var startInfo = new ProcessStartInfo
			{
				Verb = "runas",
				FileName = Assembly.GetExecutingAssembly().Location,
				Arguments = $"\"{targetPath}\" --elevated --left {Left} --top {Top}"
			};

			switch (targetApi)
			{
				case Api.D3D9:
					startInfo.Arguments += " --api d3d9";
					break;
				case Api.DXGI:
					startInfo.Arguments += " --api dxgi";
					break;
				case Api.OpenGL:
					startInfo.Arguments += " --api opengl";
					break;
				case Api.Vulkan:
					startInfo.Arguments += " --api vulkan";
					break;
			}

			if (isFinished)
			{
				startInfo.Arguments += " --finished";
			}

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

		void InstallStep0()
		{
			if (!isElevated && !IsWritable(Path.GetDirectoryName(targetPath)))
			{
				RestartWithElevatedPrivileges();
			}
			else
			{
				RunTaskWithExceptionHandling(InstallStep1);
			}
		}
		void InstallStep1()
		{
			UpdateStatus("Analyzing executable ...");

			string targetPathUnrealEngine = Path.Combine(Path.GetDirectoryName(targetPath), Path.GetFileNameWithoutExtension(targetPath), "Binaries", "Win64", Path.GetFileNameWithoutExtension(targetPath) + "-Win64-Shipping" + Path.GetExtension(targetPath));
			if (File.Exists(targetPathUnrealEngine))
			{
				targetPath = targetPathUnrealEngine;
			}

			var info = FileVersionInfo.GetVersionInfo(targetPath);
			targetName = info.FileDescription;
			if (targetName is null || targetName.Trim().Length == 0)
			{
				targetName = Path.GetFileNameWithoutExtension(targetPath);
				if (char.IsLower(targetName[0]))
				{
					targetName = CultureInfo.CurrentCulture.TextInfo.ToTitleCase(targetName);
				}
			}

			var peInfo = new PEInfo(targetPath);
			is64Bit = peInfo.Type == PEInfo.BinaryType.IMAGE_FILE_MACHINE_AMD64;

			bool isApiD3D9 = false;
			bool isApiDXGI = false;
			bool isApiOpenGL = false;
			bool isApiVulkan = false;

			// Check whether the API is specified in the compatibility list, in which case setup can continue right away
			var executableName = Path.GetFileName(targetPath);
			if (compatibilityIni != null && compatibilityIni.HasValue(executableName, "RenderApi"))
			{
				string api = compatibilityIni.GetString(executableName, "RenderApi");

				if (api == "D3D8" || api == "D3D9")
				{
					isApiD3D9 = true;
				}
				else if (api == "D3D10" || api == "D3D11" || api == "D3D12" || api == "DXGI")
				{
					isApiDXGI = true;
				}
				else if (api == "OpenGL")
				{
					isApiOpenGL = true;
				}
				else if (api == "Vulkan")
				{
					isApiVulkan = true;
				}
			}
			else
			{
				bool isApiD3D8 = peInfo.Modules.Any(s => s.StartsWith("d3d8", StringComparison.OrdinalIgnoreCase));
				isApiD3D9 = isApiD3D8 || peInfo.Modules.Any(s => s.StartsWith("d3d9", StringComparison.OrdinalIgnoreCase));
				isApiDXGI = peInfo.Modules.Any(s => s.StartsWith("dxgi", StringComparison.OrdinalIgnoreCase) || s.StartsWith("d3d1", StringComparison.OrdinalIgnoreCase) || s.Contains("GFSDK")); // Assume DXGI when GameWorks SDK is in use
				isApiOpenGL = peInfo.Modules.Any(s => s.StartsWith("opengl32", StringComparison.OrdinalIgnoreCase));
				isApiVulkan = peInfo.Modules.Any(s => s.StartsWith("vulkan-1", StringComparison.OrdinalIgnoreCase));

				if (isApiD3D9 && isApiDXGI)
				{
					isApiD3D9 = false; // Prefer DXGI over D3D9
				}
				if (isApiD3D8 && !isHeadless)
				{
					UpdateStatus("Waiting for user confirmation ...");

					Dispatcher.Invoke(() =>
					{
						MessageBox.Show(this, "It looks like the target application uses Direct3D 8.\nIn order to use ReShade you'll have to download an additional wrapper from 'https://github.com/crosire/d3d8to9/releases' which converts all API calls to Direct3D 9.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
					});
				}
				if (isApiDXGI && isApiVulkan)
				{
					isApiDXGI = false; // Prefer Vulkan over Direct3D 12
				}
				if (isApiOpenGL && (isApiD3D8 || isApiD3D9 || isApiDXGI || isApiVulkan))
				{
					isApiOpenGL = false; // Prefer Vulkan and Direct3D over OpenGL
				}
			}

			if (isHeadless)
			{
				if (isApiD3D9)
				{
					targetApi = Api.D3D9;
				}
				else if (isApiDXGI)
				{
					targetApi = Api.DXGI;
				}
				else if (isApiOpenGL)
				{
					targetApi = Api.OpenGL;
				}
				else if (isApiVulkan)
				{
					targetApi = Api.Vulkan;
				}

				InstallStep2();
				return;
			}

			Dispatcher.Invoke(() =>
			{
				var page = new SelectApiPage(targetName);
				page.ApiD3D9.IsChecked = isApiD3D9;
				page.ApiDXGI.IsChecked = isApiDXGI;
				page.ApiOpenGL.IsChecked = isApiOpenGL;
				page.ApiVulkan.IsChecked = isApiVulkan;

				CurrentPage.Navigate(page);
			});
		}
		void InstallStep2()
		{
			UpdateStatus("Checking installation status ...");

			var basePath = Path.GetDirectoryName(targetPath);
			var executableName = Path.GetFileName(targetPath);
			if (compatibilityIni != null && compatibilityIni.HasValue(executableName, "InstallTarget"))
			{
				basePath = Path.Combine(basePath, compatibilityIni.GetString(executableName, "InstallTarget"));

				var globalConfig = new IniFile(Path.Combine(Path.GetDirectoryName(targetPath), "ReShade.ini"));
				globalConfig.SetValue("INSTALL", "BasePath", basePath);
				globalConfig.SaveFile();
			}

			configPath = Path.Combine(basePath, "ReShade.ini");

			var isReShade = false;

			if (targetApi != Api.Vulkan)
			{
				switch (targetApi)
				{
					case Api.D3D9:
						modulePath = "d3d9.dll";
						break;
					case Api.DXGI:
						modulePath = "dxgi.dll";
						break;
					case Api.OpenGL:
						modulePath = "opengl32.dll";
						break;
					default: // No API selected, abort immediately
						UpdateStatusAndFinish(false, "Could not detect the rendering API used by the application.");
						return;
				}

				modulePath = Path.Combine(basePath, modulePath);

				var configPathAlt = Path.ChangeExtension(modulePath, ".ini");
				if (File.Exists(configPathAlt) && !File.Exists(configPath))
				{
					configPath = configPathAlt;
				}

				if (ModuleExists(modulePath, out isReShade))
				{
					if (isReShade)
					{
						if (isHeadless)
						{
							UpdateStatusAndFinish(false, "Existing ReShade installation found. Please uninstall it first.");
							return;
						}

						Dispatcher.Invoke(() =>
						{
							var page = new SelectUninstallPage();

							CurrentPage.Navigate(page);
						});
					}
					else
					{
						UpdateStatusAndFinish(false, Path.GetFileName(modulePath) + " already exists, but does not belong to ReShade.\nPlease make sure this is not a system file required by the game.");
					}
					return;
				}
			}
			else
			{
				var moduleName = is64Bit ? "ReShade64" : "ReShade32";
				modulePath = Path.Combine(commonPath, moduleName, moduleName + ".dll");

				var appConfig = new IniFile(Path.Combine(commonPath, "ReShadeApps.ini"));
				if (appConfig.GetValue(string.Empty, "Apps", out string[] appKeys) && appKeys.Contains(targetPath))
				{
					if (isHeadless)
					{
						UpdateStatusAndFinish(false, "Existing ReShade installation found. Please uninstall it first.");
					}
					else
					{
						Dispatcher.Invoke(() =>
						{
							var page = new SelectUninstallPage();

							CurrentPage.Navigate(page);
						});
					}
					return;
				}
			}

			foreach (string conflictingModuleName in new[] { "d3d9.dll", "dxgi.dll", "opengl32.dll" })
			{
				string conflictingModulePath = Path.Combine(basePath, conflictingModuleName);

				if (ModuleExists(conflictingModulePath, out isReShade) && isReShade)
				{
					if (isHeadless)
					{
						UpdateStatusAndFinish(false, "Existing ReShade installation for another API found.\nMultiple simultaneous ReShade installations are not supported. Please uninstall the existing one first.");
					}
					else
					{
						Dispatcher.Invoke(() =>
						{
							var page = new SelectUninstallPage();

							CurrentPage.Navigate(page);
						});
					}
					return;
				}
			}

			InstallStep3();
		}
		void InstallStep3()
		{
			UpdateStatus("Installing ReShade ...");

			string basePath = Path.GetDirectoryName(configPath);

			// Delete any existing and conflicting ReShade installations first
			foreach (string conflictingModuleName in new[] { "d3d9.dll", "dxgi.dll", "opengl32.dll" })
			{
				string conflictingModulePath = Path.Combine(basePath, conflictingModuleName);

				try
				{
					if (ModuleExists(conflictingModulePath, out bool isReShade) && isReShade)
					{
						File.Delete(conflictingModulePath);
					}
				}
				catch
				{
					// Ignore errors
					continue;
				}
			}

			if (targetApi == Api.Vulkan)
			{
				try
				{
					// Unregister any layers from previous ReShade installations
					using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Khronos\Vulkan\ExplicitLayers", true))
					{
						key.DeleteValue(Path.Combine(commonPath, "ReShade32", "ReShade32.json"), false);
						key.DeleteValue(Path.Combine(commonPath, "ReShade64", "ReShade64.json"), false);
					}
					using (RegistryKey key = Registry.CurrentUser.OpenSubKey(@"Software\Khronos\Vulkan\ImplicitLayers", true))
					{
						key.DeleteValue(Path.Combine(commonPath, "ReShade.json"), false);
						key.DeleteValue(Path.Combine(commonPath, "VkLayer_override.json"), false);
						key.DeleteValue(Path.Combine(commonPath, "ReShade32_vk_override_layer.json"), false);
						key.DeleteValue(Path.Combine(commonPath, "ReShade64_vk_override_layer.json"), false);
					}
					using (RegistryKey key = Registry.LocalMachine.OpenSubKey(@"Software\Khronos\Vulkan\ImplicitLayers", true))
					{
						key.DeleteValue(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "ReShade", "ReShade32.json"), false);
						key.DeleteValue(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), "ReShade", "ReShade64.json"), false);
					}
				}
				catch
				{
					// Ignore errors
				}

				foreach (string layerModuleName in new[] { "ReShade32", "ReShade64" })
				{
					string parentPath = Path.Combine(commonPath, layerModuleName);
					string layerModulePath = Path.Combine(parentPath, layerModuleName + ".dll");
					string layerManifestPath = Path.Combine(parentPath, layerModuleName + ".json");

					try
					{
						if (!Directory.Exists(parentPath))
						{
							Directory.CreateDirectory(parentPath);
						}

						var module = zip.GetEntry(Path.GetFileName(layerModulePath));
						if (module == null)
						{
							throw new FileFormatException("Setup archive is missing ReShade DLL file.");
						}

						module.ExtractToFile(layerModulePath, true);
					}
					catch (Exception ex)
					{
						UpdateStatusAndFinish(false, "Failed to install " + Path.GetFileName(layerModulePath) + ":\n" + ex.Message);
						return;
					}

					try
					{
						var manifest = zip.GetEntry(Path.GetFileName(layerManifestPath));
						if (manifest == null)
						{
							throw new FileFormatException("Setup archive is missing Vulkan layer manifest file.");
						}

						manifest.ExtractToFile(layerManifestPath, true);

						// Register this layer manifest
						using (RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
						{
							key.SetValue(layerManifestPath, 0, RegistryValueKind.DWord);
						}
					}
					catch (Exception ex)
					{
						UpdateStatusAndFinish(false, "Failed to install Vulkan layer manifest:\n" + ex.Message);
						return;
					}
				}

				var appConfig = new IniFile(Path.Combine(commonPath, "ReShadeApps.ini"));
				if (appConfig.GetValue(string.Empty, "Apps", out string[] appKeys) == false || !appKeys.Contains(targetPath))
				{
					var appKeysList = appKeys != null ? appKeys.ToList() : new List<string>();
					appKeysList.Add(targetPath);
					appConfig.SetValue(string.Empty, "Apps", appKeysList.ToArray());
					appConfig.SaveFile();
				}
			}
			else
			{
				string parentPath = Path.GetDirectoryName(modulePath);

				try
				{
					if (!Directory.Exists(parentPath))
					{
						Directory.CreateDirectory(parentPath);
					}

					var module = zip.GetEntry(is64Bit ? "ReShade64.dll" : "ReShade32.dll");
					if (module == null)
					{
						throw new FileFormatException("Setup archive is missing ReShade DLL file.");
					}

					module.ExtractToFile(modulePath, true);
				}
				catch (Exception ex)
				{
					UpdateStatusAndFinish(false, "Failed to install " + Path.GetFileName(modulePath) + ":\n" + ex.Message);
					return;
				}

				// Create a default log file for troubleshooting
				File.WriteAllText(Path.Combine(basePath, "ReShade.log"), @"
If you are reading this after launching the game at least once, it likely means ReShade was not loaded by the game.

In that event here are some steps you can try to resolve this:

1) Make sure this file and the related DLL are really in the same directory as the game executable.
   If that is the case and it does not work regardless, check if there is a 'bin' directory, move them there and try again.

2) Try running the game with elevated user permissions by doing a right click on its executable and choosing 'Run as administrator'.

3) If the game crashes, try disabling all game overlays (like Origin), recording software (like Fraps), FPS displaying software,
   GPU overclocking and tweaking software and other proxy DLLs (like ENB, Helix or Umod).

4) If none of the above helps, you can get support on the forums at https://reshade.me/forum. But search for your problem before
   creating a new topic, as somebody else may have already found a solution.
");
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
					UpdateStatusAndFinish(false, "Failed to install " + Path.GetFileName(configPath) + ":\n" + ex.Message);
					return;
				}
			}

			// Add default configuration
			var config = new IniFile(configPath);
			if (compatibilityIni != null && !config.HasValue("GENERAL", "PreprocessorDefinitions"))
			{
				string depthReversed = compatibilityIni.GetString(targetName, "DepthReversed", "0");
				string depthUpsideDown = compatibilityIni.GetString(targetName, "DepthUpsideDown", "0");
				string depthLogarithmic = compatibilityIni.GetString(targetName, "DepthLogarithmic", "0");
				if (!compatibilityIni.HasValue(targetName, "DepthReversed"))
				{
					var info = FileVersionInfo.GetVersionInfo(targetPath);
					if (info.LegalCopyright != null)
					{
						Match match = new Regex("(20[0-9]{2})", RegexOptions.RightToLeft).Match(info.LegalCopyright);
						if (match.Success && int.TryParse(match.Groups[1].Value, out int year))
						{
							// Modern games usually use reversed depth
							depthReversed = (year >= 2012) ? "1" : "0";
						}
					}
				}

				config.SetValue("GENERAL", "PreprocessorDefinitions",
					"RESHADE_DEPTH_LINEARIZATION_FAR_PLANE=1000.0",
					"RESHADE_DEPTH_INPUT_IS_UPSIDE_DOWN=" + depthUpsideDown,
					"RESHADE_DEPTH_INPUT_IS_REVERSED=" + depthReversed,
					"RESHADE_DEPTH_INPUT_IS_LOGARITHMIC=" + depthLogarithmic);

				if (compatibilityIni.HasValue(targetName, "DepthCopyBeforeClears") ||
					compatibilityIni.HasValue(targetName, "UseAspectRatioHeuristics"))
				{
					config.SetValue("DEPTH", "DepthCopyBeforeClears",
						compatibilityIni.GetString(targetName, "DepthCopyBeforeClears", "0"));
					config.SetValue("DEPTH", "UseAspectRatioHeuristics",
						compatibilityIni.GetString(targetName, "UseAspectRatioHeuristics", "1"));
				}
			}

			// Update old configurations to new format
			if (!config.HasValue("INPUT", "KeyOverlay") && config.HasValue("INPUT", "KeyMenu"))
			{
				config.RenameValue("INPUT", "KeyMenu", "KeyOverlay");

				config.RenameValue("GENERAL", "CurrentPresetPath", "PresetPath");

				config.RenameValue("GENERAL", "ShowFPS", "OVERLAY", "ShowFPS");
				config.RenameValue("GENERAL", "ShowClock", "OVERLAY", "ShowClock");
				config.RenameValue("GENERAL", "ShowFrameTime", "OVERLAY", "ShowFrameTime");
				config.RenameValue("GENERAL", "ShowScreenshotMessage", "OVERLAY", "ShowScreenshotMessage");
				config.RenameValue("GENERAL", "FPSPosition", "OVERLAY", "FPSPosition");
				config.RenameValue("GENERAL", "ClockFormat", "OVERLAY", "ClockFormat");
				config.RenameValue("GENERAL", "NoFontScaling", "OVERLAY", "NoFontScaling");
				config.RenameValue("GENERAL", "SaveWindowState", "OVERLAY", "SaveWindowState");
				config.RenameValue("GENERAL", "TutorialProgress", "OVERLAY", "TutorialProgress");
				config.RenameValue("GENERAL", "VariableUIHeight", "OVERLAY", "VariableListHeight");
				config.RenameValue("GENERAL", "NewVariableUI", "OVERLAY", "VariableListUseTabs");
				config.RenameValue("GENERAL", "ScreenshotFormat", "SCREENSHOT", "FileFormat");
				config.RenameValue("GENERAL", "ScreenshotSaveBefore", "SCREENSHOT", "SaveBeforeShot");
				config.RenameValue("GENERAL", "ScreenshotSaveUI", "SCREENSHOT", "SaveOverlayShot");
				config.RenameValue("GENERAL", "ScreenshotPath", "SCREENSHOT", "SavePath");
				config.RenameValue("GENERAL", "ScreenshotIncludePreset", "SCREENSHOT", "SavePresetFile");
			}

			if (!config.HasValue("DEPTH"))
			{
				if (config.HasValue("D3D9"))
				{
					config.RenameValue("D3D9", "DisableINTZ", "DEPTH", "DisableINTZ");
					config.RenameValue("D3D9", "DepthCopyBeforeClears", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("D3D9", "DepthCopyAtClearIndex", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("D3D9", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
				else if (config.HasValue("DX9_BUFFER_DETECTION"))
				{
					config.RenameValue("DX9_BUFFER_DETECTION", "DisableINTZ", "DEPTH", "DisableINTZ");
					config.RenameValue("DX9_BUFFER_DETECTION", "PreserveDepthBuffer", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("DX9_BUFFER_DETECTION", "PreserveDepthBufferIndex", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("DX9_BUFFER_DETECTION", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}

				if (config.HasValue("D3D10"))
				{
					config.RenameValue("D3D10", "DepthCopyBeforeClears", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("D3D10", "DepthCopyAtClearIndex", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("D3D10", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
				else if (config.HasValue("DX10_BUFFER_DETECTION"))
				{
					config.RenameValue("DX10_BUFFER_DETECTION", "DepthBufferRetrievalMode", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("DX10_BUFFER_DETECTION", "DepthBufferClearingNumber", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("DX10_BUFFER_DETECTION", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}

				if (config.HasValue("D3D11"))
				{
					config.RenameValue("D3D11", "DepthCopyBeforeClears", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("D3D11", "DepthCopyAtClearIndex", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("D3D11", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
				else if (config.HasValue("DX11_BUFFER_DETECTION"))
				{
					config.RenameValue("DX11_BUFFER_DETECTION", "DepthBufferRetrievalMode", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("DX11_BUFFER_DETECTION", "DepthBufferClearingNumber", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("DX11_BUFFER_DETECTION", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}

				if (config.HasValue("D3D12"))
				{
					config.RenameValue("D3D12", "DepthCopyBeforeClears", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("D3D12", "DepthCopyAtClearIndex", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("D3D12", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
				else if (config.HasValue("DX12_BUFFER_DETECTION"))
				{
					config.RenameValue("DX12_BUFFER_DETECTION", "DepthBufferRetrievalMode", "DEPTH", "DepthCopyBeforeClears");
					config.RenameValue("DX12_BUFFER_DETECTION", "DepthBufferClearingNumber", "DEPTH", "DepthCopyAtClearIndex");
					config.RenameValue("DX12_BUFFER_DETECTION", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}

				if (config.HasValue("OPENGL"))
				{
					config.RenameValue("OPENGL", "ReserveTextureNames", "APP", "ReserveTextureNames");
					config.RenameValue("OPENGL", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}

				if (config.HasValue("VULKAN"))
				{
					config.RenameValue("VULKAN", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
				else if (config.HasValue("VULKAN_BUFFER_DETECTION"))
				{
					config.RenameValue("VULKAN_BUFFER_DETECTION", "UseAspectRatioHeuristics", "DEPTH", "UseAspectRatioHeuristics");
				}
			}

			if (!config.HasValue("SCREENSHOT"))
			{
				config.RenameValue("SCREENSHOTS", "FileFormat", "SCREENSHOT", "FileFormat");
				config.RenameValue("SCREENSHOTS", "SaveBeforeShot", "SCREENSHOT", "SaveBeforeShot");
				config.RenameValue("SCREENSHOTS", "SaveOverlayShot", "SCREENSHOT", "SaveOverlayShot");
				config.RenameValue("SCREENSHOTS", "SavePath", "SCREENSHOT", "SavePath");
				config.RenameValue("SCREENSHOTS", "SavePresetFile", "SCREENSHOT", "SavePresetFile");
			}

			if (!config.HasValue("SCREENSHOT", "FileNaming") && config.HasValue("SCREENSHOT", "FileNamingFormat"))
			{
				if (int.TryParse(config.GetString("SCREENSHOT", "FileNamingFormat", "0"), out int formatIndex))
				{
					if (formatIndex == 0)
						config.SetValue("SCREENSHOT", "FileNaming", "%AppName% %Date% %Time%");
					else if (formatIndex == 1)
						config.SetValue("SCREENSHOT", "FileNaming", "%AppName% %Date% %Time% %PresetName%");
				}
			}

			if (!config.HasValue("GENERAL", "PresetPath") && config.HasValue("GENERAL", "CurrentPreset"))
			{
				if (config.GetValue("GENERAL", "PresetFiles", out string[] presetFiles) &&
					int.TryParse(config.GetString("GENERAL", "CurrentPreset", "0"), out int presetIndex) && presetIndex < presetFiles.Length)
				{
					config.SetValue("GENERAL", "PresetPath", presetFiles[presetIndex]);
				}
			}

			if (!config.HasValue("GENERAL", "PresetTransitionDuration") && config.HasValue("GENERAL", "PresetTransitionDelay"))
			{
				config.RenameValue("GENERAL", "PresetTransitionDelay", "GENERAL", "PresetTransitionDuration");
			}

			// Always add input section
			if (!config.HasValue("INPUT"))
			{
				config.SetValue("INPUT", "KeyOverlay", "36,0,0,0");
			}

			config.SaveFile();

			// Only show the selection dialog if there are actually packages to choose
			if (!isHeadless && packagesIni != null && packagesIni.GetSections().Length != 0)
			{
				presetPath = config.GetString("GENERAL", "PresetPath", string.Empty);

				if (isUpdate)
				{
					InstallStep4();
				}
				else
				{
					Dispatcher.Invoke(() =>
					{
						var page = new SelectPresetPage();
						page.FileName = presetPath;

						CurrentPage.Navigate(page);
					});
				}
				return;
			}

			// Add default search paths if no config exists
			if (!config.HasValue("GENERAL", "EffectSearchPaths") && !config.HasValue("GENERAL", "TextureSearchPaths"))
			{
				WriteSearchPaths(".\\", ".\\");
			}

			InstallStep8();
		}
		void InstallStep4()
		{
			var effectFiles = new List<string>();

			if (!string.IsNullOrEmpty(presetPath) && File.Exists(presetPath))
			{
				var config = new IniFile(configPath);
				config.SetValue("GENERAL", "PresetPath", presetPath);
				config.SaveFile();

				var preset = new IniFile(presetPath);

				if (preset.GetValue(string.Empty, "Techniques", out string[] techniques))
				{
					foreach (string technique in techniques)
					{
						var filenameIndex = technique.IndexOf('@');
						if (filenameIndex > 0)
						{
							string filename = technique.Substring(filenameIndex + 1);

							effectFiles.Add(filename);
						}
					}
				}
			}

			Dispatcher.Invoke(() =>
			{
				var page = new SelectPackagesPage(packagesIni, effectFiles);

				CurrentPage.Navigate(page);
			});
		}
		void InstallStep5()
		{
			package = packages.Dequeue();
			downloadPath = Path.GetTempFileName();

			UpdateStatus("Downloading " + package.PackageName + " from " + package.DownloadUrl + " ...");

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) =>
			{
				if (e.Error != null)
				{
					UpdateStatusAndFinish(false, "Failed to download from " + package.DownloadUrl + ":\n" + e.Error.Message);
				}
				else
				{
					InstallStep6();
				}
			};

			client.DownloadProgressChanged += (object sender, DownloadProgressChangedEventArgs e) =>
			{
				// Avoid negative percentage values
				if (e.TotalBytesToReceive > 0)
				{
					UpdateStatus("Downloading " + package.PackageName + " ... (" + ((100 * e.BytesReceived) / e.TotalBytesToReceive) + "%)");
				}
			};

			try
			{
				client.DownloadFileAsync(new Uri(package.DownloadUrl), downloadPath);
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Failed to download from " + package.DownloadUrl + ":\n" + ex.Message);
			}
		}
		void InstallStep6()
		{
			UpdateStatus("Extracting " + package.PackageName + " ...");

			tempPath = Path.Combine(Path.GetTempPath(), "reshade-shaders");

			string basePath = Path.GetDirectoryName(configPath);
			targetPathEffects = Path.Combine(basePath, package.InstallPath);
			targetPathTextures = Path.Combine(basePath, package.TextureInstallPath);

			try
			{
				// Delete existing directories since extraction fails if the target is not empty
				if (Directory.Exists(tempPath))
				{
					Directory.Delete(tempPath, true);
				}

				ZipFile.ExtractToDirectory(downloadPath, tempPath);

				// First check for a standard directory name
				tempPathEffects = Directory.GetDirectories(tempPath, "Shaders", SearchOption.AllDirectories).FirstOrDefault();
				tempPathTextures = Directory.GetDirectories(tempPath, "Textures", SearchOption.AllDirectories).FirstOrDefault();

				// If that does not exist, look for the first directory that contains shaders/textures
				effects = Directory.GetFiles(tempPath, "*.fx", SearchOption.AllDirectories);
				if (tempPathEffects == null)
				{
					tempPathEffects = effects.Select(x => Path.GetDirectoryName(x)).OrderBy(x => x.Length).FirstOrDefault();
				}
				if (tempPathTextures == null)
				{
					string[] tempTextureExtensions = { "*.png", "*.jpg", "*.jpeg" };

					foreach (string extension in tempTextureExtensions)
					{
						string path = Directory.GetFiles(tempPath, extension, SearchOption.AllDirectories).Select(x => Path.GetDirectoryName(x)).OrderBy(x => x.Length).FirstOrDefault();
						if (!string.IsNullOrEmpty(path))
						{
							tempPathTextures = tempPathTextures != null ? tempPathTextures.Union(path).ToString() : path;
						}
					}
				}

				// Delete denied effects
				if (package.DenyEffectFiles != null)
				{
					var denyEffectFiles = effects.Where(x => package.DenyEffectFiles.Contains(Path.GetFileName(x)));

					foreach (string filePath in denyEffectFiles)
					{
						File.Delete(filePath);
					}

					effects = effects.Except(denyEffectFiles).ToArray();
				}

				// Show file selection dialog
				if (!isHeadless && package.Enabled == null)
				{
					effects = effects.Select(x => targetPathEffects + x.Remove(0, tempPathEffects.Length)).ToArray();

					Dispatcher.Invoke(() =>
					{
						var page = new SelectEffectsPage(package.PackageName, effects);

						CurrentPage.Navigate(page);
					});
					return;
				}
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Failed to extract " + package.PackageName + ":\n" + ex.Message);
				return;
			}

			InstallStep7();
		}
		void InstallStep7()
		{
			try
			{
				// Move only the relevant files to the target
				if (tempPathEffects != null)
				{
					MoveFiles(tempPathEffects, targetPathEffects);
				}
				if (tempPathTextures != null)
				{
					MoveFiles(tempPathTextures, targetPathTextures);
				}

				File.Delete(downloadPath);
				Directory.Delete(tempPath, true);
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Failed to extract " + package.PackageName + ":\n" + ex.Message);
				return;
			}

			WriteSearchPaths(package.InstallPath, package.TextureInstallPath);

			if (packages.Count != 0)
			{
				InstallStep5();
			}
			else
			{
				InstallStep8();
			}
		}
		void InstallStep8()
		{
			UpdateStatusAndFinish(true, "Successfully installed ReShade." + (isHeadless ? string.Empty : "\nClick the \"Finish\" button to exit the setup tool."));
		}

		void UninstallStep0()
		{
			if (targetApi == Api.Vulkan)
			{
				var appConfig = new IniFile(Path.Combine(commonPath, "ReShadeApps.ini"));
				if (appConfig.GetValue(string.Empty, "Apps", out string[] appKeys))
				{
					var appKeysList = appKeys.ToList();
					appKeysList.Remove(targetPath);
					appConfig.SetValue(string.Empty, "Apps", appKeysList.ToArray());

					if (appKeysList.Count == 0)
					{
						try
						{
							Directory.Delete(commonPath, true);

							using (RegistryKey key = Registry.CurrentUser.CreateSubKey(@"Software\Khronos\Vulkan\ImplicitLayers"))
							{
								key.DeleteValue(Path.Combine(commonPath, "ReShade32", "ReShade32.json"), false);
								key.DeleteValue(Path.Combine(commonPath, "ReShade64", "ReShade64.json"), false);
							}
						}
						catch (Exception ex)
						{
							UpdateStatusAndFinish(false, "Failed to delete Vulkan layer manifest:\n" + ex.Message);
							return;
						}
					}
					else
					{
						appConfig.SaveFile();
					}
				}
			}

			try
			{
				string basePath = Path.GetDirectoryName(configPath);

				if (targetApi != Api.Vulkan)
				{
					File.Delete(modulePath);
				}

				if (File.Exists(configPath))
				{
					File.Delete(configPath);
				}

				if (File.Exists(Path.Combine(basePath, "ReShade.log")))
				{
					File.Delete(Path.Combine(basePath, "ReShade.log"));
				}

				if (File.Exists(Path.Combine(basePath, "ReShadeGUI.ini")))
				{
					File.Delete(Path.Combine(basePath, "ReShadeGUI.ini"));
				}

				if (Directory.Exists(Path.Combine(basePath, "reshade-shaders")))
				{
					Directory.Delete(Path.Combine(basePath, "reshade-shaders"), true);
				}

				// Delete all other existing ReShade installations too
				foreach (string conflictingModuleName in new[] { "d3d9.dll", "dxgi.dll", "opengl32.dll" })
				{
					string conflictingModulePath = Path.Combine(basePath, conflictingModuleName);

					if (ModuleExists(conflictingModulePath, out bool isReShade) && isReShade)
					{
						File.Delete(conflictingModulePath);
					}
				}
			}
			catch (Exception ex)
			{
				UpdateStatusAndFinish(false, "Failed to delete some ReShade files:\n" + ex.Message);
				return;
			}

			UninstallStep1();
		}
		void UninstallStep1()
		{
			UpdateStatusAndFinish(true, "Successfully uninstalled ReShade." + (isHeadless ? string.Empty : "\nClick the \"Finish\" button to exit the setup tool."));
		}

		void OnWindowInit(object sender, EventArgs e)
		{
			AeroGlass.HideIcon(this);
			AeroGlass.HideSystemMenu(this, targetPath != null);
		}

		void OnNextButtonClick(object sender, RoutedEventArgs e)
		{
			if (isFinished)
			{
				Close();
				return;
			}

			if (CurrentPage.Content is SelectAppPage appPage)
			{
				appPage.Cancel();

				targetPath = appPage.FileName;

				InstallStep0();
				return;
			}

			if (CurrentPage.Content is SelectApiPage apiPage)
			{
				if (apiPage.ApiD3D9.IsChecked == true)
				{
					targetApi = Api.D3D9;
				}
				if (apiPage.ApiDXGI.IsChecked == true)
				{
					targetApi = Api.DXGI;
				}
				if (apiPage.ApiOpenGL.IsChecked == true)
				{
					targetApi = Api.OpenGL;
				}
				if (apiPage.ApiVulkan.IsChecked == true)
				{
					targetApi = Api.Vulkan;
				}

				RunTaskWithExceptionHandling(InstallStep2);
				return;
			}

			if (CurrentPage.Content is SelectUninstallPage uninstallPage)
			{
				if (uninstallPage.UpdateButton.IsChecked == true)
				{
					isUpdate = true;

					RunTaskWithExceptionHandling(InstallStep3);
				}
				else
				{
					RunTaskWithExceptionHandling(UninstallStep0);
				}
				return;
			}

			if (CurrentPage.Content is SelectPresetPage presetPage)
			{
				presetPath = presetPage.FileName;

				RunTaskWithExceptionHandling(InstallStep4);
				return;
			}

			if (CurrentPage.Content is SelectPackagesPage packagesPage)
			{
				packages = new Queue<EffectPackage>(packagesPage.EnabledItems);

				if (packages.Count != 0)
				{
					RunTaskWithExceptionHandling(InstallStep5);
				}
				else
				{
					RunTaskWithExceptionHandling(InstallStep8);
				}
				return;
			}

			if (CurrentPage.Content is SelectEffectsPage effectsPage)
			{
				RunTaskWithExceptionHandling(() =>
				{
					// Delete all unselected effect files before moving
					foreach (string filePath in effects.Except(effectsPage.EnabledItems.Select(x => x.FilePath)))
					{
						File.Delete(tempPathEffects + filePath.Remove(0, targetPathEffects.Length));
					}

					InstallStep7();
				});
				return;
			}
		}

		void OnCancelButtonClick(object sender, RoutedEventArgs e)
		{
			if (CurrentPage.Content is SelectAppPage appPage)
			{
				appPage.Cancel();
			}

			if (CurrentPage.Content is SelectPresetPage)
			{
				presetPath = null;

				RunTaskWithExceptionHandling(InstallStep4);
				return;
			}

			if (CurrentPage.Content is SelectPackagesPage)
			{
				RunTaskWithExceptionHandling(InstallStep8);
				return;
			}

			if (CurrentPage.Content is SelectEffectsPage)
			{
				RunTaskWithExceptionHandling(InstallStep7);
				return;
			}

			if (isFinished)
			{
				ResetStatus();
				return;
			}

			Close();
		}

		void OnCurrentPageNavigated(object sender, NavigationEventArgs e)
		{
			NextButton.Content = isFinished ? "_Finish" : "_Next";
			CancelButton.Content = isFinished ? "_Back" : (e.Content is SelectPresetPage || e.Content is SelectPackagesPage || e.Content is SelectEffectsPage) ? "_Skip" : "_Cancel";

			CancelButton.IsEnabled = !(e.Content is StatusPage) || isFinished;

			if (!(e.Content is SelectAppPage))
			{
				NextButton.IsEnabled = isFinished || !(e.Content is StatusPage);
			}
		}
	}
}
