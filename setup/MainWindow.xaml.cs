/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using Microsoft.Win32;
using ReShade.Setup.Pages;
using ReShade.Setup.Utilities;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Principal;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Navigation;

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
		string targetPath;
		string targetName;
		string configPath;
		string modulePath;
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
		bool? addonLoadSupport;

		public MainWindow()
		{
			InitializeComponent();

			var assembly = Assembly.GetExecutingAssembly();

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
				packagesIni = new IniFile(assembly.GetManifestResourceStream("ReShade.Setup.EffectPackages.ini"));
				compatibilityIni = new IniFile(assembly.GetManifestResourceStream("ReShade.Setup.Compatibility.ini"));

				// Validate archive contains the ReShade DLLs
				if (zip.GetEntry("ReShade32.dll") == null || zip.GetEntry("ReShade64.dll") == null)
				{
					throw new InvalidDataException();
				}

				if (zip.GetEntry("ReShade32_official.dll") == null || zip.GetEntry("ReShade64_official.dll") == null)
				{
					addonLoadSupport = null;
				}
				else
				{
					addonLoadSupport = true;
				}
			}
			catch
			{
				MessageBox.Show("This setup archive is corrupted! Please download from https://reshade.me again.");
				Environment.Exit(1);
				return;
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
					InstallStep7();
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
				return;
			}
			else if (isHeadless)
			{
				UpdateStatusAndFinish(false, "No target application was provided.");
				return;
			}

			NextButton.IsEnabled = false;

			appPage.PathBox.TextChanged += (sender2, e2) => NextButton.IsEnabled = !string.IsNullOrEmpty(appPage.FileName) && Path.GetExtension(appPage.FileName) == ".exe" && File.Exists(appPage.FileName);

			ResetStatus();
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

		static bool ReShadeExists(string path, out bool isReShade)
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
		}

		void ResetStatus()
		{
			isFinished = false;

			targetApi = Api.Unknown;
			targetPath = targetName = configPath = modulePath = tempPath = tempPathEffects = tempPathTextures = targetPathEffects = targetPathTextures = downloadPath = null;
			packages = null; effects = null; package = null;

			Dispatcher.Invoke(() =>
			{
				CurrentPage.Navigate(appPage);

				Title = "ReShade Setup v" + Assembly.GetExecutingAssembly().GetName().Version.ToString(3);
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

			if (targetApi == Api.D3D9)
			{
				startInfo.Arguments += " --api d3d9";
			}
			
			if (targetApi == Api.DXGI)
			{
				startInfo.Arguments += " --api dxgi";
			}
			
			if (targetApi == Api.OpenGL)
			{
				startInfo.Arguments += " --api opengl";
			}
			
			if (targetApi == Api.Vulkan)
			{
				startInfo.Arguments += " --api vulkan";
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
				InstallStep1();
			}
		}
		void InstallStep1()
		{
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
			}

			UpdateStatus("Analyzing executable ...");

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
					MessageBox.Show(this, "It looks like the target application uses Direct3D 8. You'll have to download an additional wrapper from 'https://github.com/crosire/d3d8to9/releases' which converts all API calls to Direct3D 9 in order to use ReShade.", "Warning", MessageBoxButton.OK, MessageBoxImage.Warning);
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

				if (addonLoadSupport != null)
				{
					page.AddonSupport.IsEnabled = true;
					page.AddonSupport.IsChecked = false;
				}
				else
				{
					page.AddonSupport.IsEnabled = false;
					page.AddonSupport.IsChecked = true;
				}

				CurrentPage.Navigate(page);
			});
		}
		void InstallStep2()
		{
			UpdateStatus("Checking installation status ...");

			var targetDir = Path.GetDirectoryName(targetPath);
			var executableName = Path.GetFileName(targetPath);
			if (compatibilityIni != null && compatibilityIni.HasValue(executableName, "InstallTarget"))
			{
				targetDir = Path.Combine(targetDir, compatibilityIni.GetString(executableName, "InstallTarget"));
			}

			configPath = Path.Combine(targetDir, "ReShade.ini");

			if (targetApi != Api.Vulkan)
			{
				if (targetApi == Api.D3D9)
				{
					modulePath = "d3d9.dll";
				}
				else if (targetApi == Api.DXGI)
				{
					modulePath = "dxgi.dll";
				}
				else if (targetApi == Api.OpenGL)
				{
					modulePath = "opengl32.dll";
				}
				else // No API selected, abort immediately
				{
					UpdateStatusAndFinish(false, "Could not detect the rendering API used by the application.");
					return;
				}

				modulePath = Path.Combine(targetDir, modulePath);

				var configPathAlt = Path.ChangeExtension(modulePath, ".ini");
				if (File.Exists(configPathAlt))
				{
					configPath = configPathAlt;
				}

				if (ReShadeExists(modulePath, out bool isReShade))
				{
					if (isReShade)
					{
						if (isHeadless)
						{
							UpdateStatusAndFinish(false, "Existing ReShade installation found. Please uninstall the existing one first.");
							return;
						}

						Dispatcher.Invoke(() =>
						{
							var page = new SelectUninstallPage();
							page.UpdateButton.Click += (sender, e) => Task.Run(InstallStep3);
							page.UninstallButton.Click += (sender, e) => Task.Run(UninstallStep0);

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
				modulePath = Path.Combine(commonPath, is64Bit ? "ReShade64" : "ReShade32", is64Bit ? "ReShade64.dll" : "ReShade32.dll");

				var overrideMetaLayerManifest = new JsonFile(Path.Combine(commonPath, is64Bit ? "ReShade64_override.json" : "ReShade32_override.json"));

				if (overrideMetaLayerManifest.GetValue("layer.app_keys", out List<string> appKeys) && appKeys.Contains(targetPath))
				{
					if (isHeadless)
					{
						UpdateStatusAndFinish(false, "Existing ReShade installation found. Please uninstall the existing one first.");
					}
					else
					{
						Dispatcher.Invoke(() =>
						{
							var page = new SelectUninstallPage();
							page.UpdateButton.Click += (sender, e) => Task.Run(InstallStep3);
							page.UninstallButton.Click += (sender, e) => Task.Run(UninstallStep0);

							CurrentPage.Navigate(page);
						});
					}
					return;
				}
			}

			if (targetApi != Api.D3D9 && ReShadeExists(Path.Combine(targetDir, "d3d9.dll"), out bool isD3D9ReShade) && isD3D9ReShade)
			{
				UpdateStatusAndFinish(false, "Existing ReShade installation for Direct3D 9 found.\nMultiple simultaneous ReShade installations are not supported. Please uninstall the existing one first.");
				return;
			}
			if (targetApi != Api.DXGI && ReShadeExists(Path.Combine(targetDir, "dxgi.dll"), out bool isDXGIReShade) && isDXGIReShade)
			{
				UpdateStatusAndFinish(false, "Existing ReShade installation for Direct3D 10/11/12 found.\nMultiple simultaneous ReShade installations are not supported. Please uninstall the existing one first.");
				return;
			}
			if (targetApi != Api.OpenGL && ReShadeExists(Path.Combine(targetDir, "opengl32.dll"), out bool isOpenGLReShade) && isOpenGLReShade)
			{
				UpdateStatusAndFinish(false, "Existing ReShade installation for OpenGL found.\nMultiple simultaneous ReShade installations are not supported. Please uninstall the existing one first.");
				return;
			}

			InstallStep3();
		}
		void InstallStep3()
		{
			UpdateStatus("Installing ReShade ...");

			// Change current directory so that "Path.GetFullPath" resolves correctly
			string basePath = Path.GetDirectoryName(configPath);
			Directory.SetCurrentDirectory(basePath);

			string parentPath = Path.GetDirectoryName(modulePath);

			string moduleName = is64Bit ? "ReShade64" : "ReShade32";

			try
			{
				if (!Directory.Exists(parentPath))
				{
					Directory.CreateDirectory(parentPath);
				}

				var module = zip.GetEntry(moduleName + (addonLoadSupport != false ? string.Empty : "_official") + ".dll");
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

			if (targetApi == Api.Vulkan)
			{
				try
				{
					var manifest = zip.GetEntry(moduleName + ".json");
					if (manifest == null)
					{
						throw new FileFormatException("Setup archive is missing Vulkan layer manifest file.");
					}

					manifest.ExtractToFile(Path.ChangeExtension(modulePath, ".json"), true);
				}
				catch (Exception ex)
				{
					UpdateStatusAndFinish(false, "Failed to install Vulkan layer manifest:\n" + ex.Message);
					return;
				}

				string overrideMetaLayerPath = Path.Combine(commonPath, moduleName + "_override.json");

				if (!File.Exists(overrideMetaLayerPath))
				{
					try
					{
						var manifest = zip.GetEntry(Path.GetFileName(overrideMetaLayerPath));
						if (manifest == null)
						{
							throw new FileFormatException("Setup archive is missing Vulkan meta layer manifest file.");
						}

						manifest.ExtractToFile(overrideMetaLayerPath, true);

						using (RegistryKey key = Registry.CurrentUser.CreateSubKey(Environment.Is64BitOperatingSystem && !is64Bit ? @"Software\Wow6432Node\Khronos\Vulkan\ImplicitLayers" : @"Software\Khronos\Vulkan\ImplicitLayers"))
						{
							key.SetValue(overrideMetaLayerPath, 0, RegistryValueKind.DWord);
						}
					}
					catch (Exception ex)
					{
						UpdateStatusAndFinish(false, "Failed to install Vulkan meta layer manifest:\n" + ex.Message);
						return;
					}
				}

				var overrideMetaLayerManifest = new JsonFile(overrideMetaLayerPath);

				overrideMetaLayerManifest.GetValue("layer.app_keys", out List<string> appKeys);
				if (!appKeys.Contains(targetPath))
				{
					appKeys.Add(targetPath);

					overrideMetaLayerManifest.SetValue("layer.app_keys", appKeys);
				}

				overrideMetaLayerManifest.GetValue("layer.override_paths", out List<string> overridePaths);
				if (!overridePaths.Contains(parentPath))
				{
					overridePaths.Add(parentPath);

					overrideMetaLayerManifest.SetValue("layer.override_paths", overridePaths);
				}

				overrideMetaLayerManifest.SaveFile();
			}
			else
			{
				// Create a default log file for troubleshooting
				File.WriteAllText(Path.ChangeExtension(modulePath, ".log"), @"
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
			if (config.HasValue("INPUT", "KeyMenu") && !config.HasValue("INPUT", "KeyOverlay"))
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

			if (config.HasValue("GENERAL", "CurrentPreset") && !config.HasValue("GENERAL", "PresetPath"))
			{
				if (config.GetValue("GENERAL", "PresetFiles", out string[] presetFiles) &&
					int.TryParse(config.GetString("GENERAL", "CurrentPreset", "0"), out int presetIndex) && presetIndex < presetFiles.Length)
				{
					config.SetValue("GENERAL", "PresetPath", presetFiles[presetIndex]);
				}
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
				Dispatcher.Invoke(() =>
				{
					var page = new SelectPackagesPage(packagesIni);

					CurrentPage.Navigate(page);
				});
				return;
			}

			// Add default search paths if no config exists
			if (!config.HasValue("GENERAL", "EffectSearchPaths") && !config.HasValue("GENERAL", "TextureSearchPaths"))
			{
				WriteSearchPaths(".\\", ".\\");
			}

			InstallStep7();
		}
		void InstallStep4()
		{
			package = packages.Dequeue();
			downloadPath = Path.GetTempFileName();

			UpdateStatus("Downloading " + package.PackageName + " from " + package.DownloadUrl + " ...");

			// Add support for TLS 1.2, so that HTTPS connection to GitHub succeeds
			ServicePointManager.SecurityProtocol |= SecurityProtocolType.Tls12;

			var client = new WebClient();

			client.DownloadFileCompleted += (object sender, System.ComponentModel.AsyncCompletedEventArgs e) =>
			{
				if (e.Error != null)
				{
					UpdateStatusAndFinish(false, "Failed to download from " + package.DownloadUrl + ":\n" + e.Error.Message);
				}
				else
				{
					InstallStep5();
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
		void InstallStep5()
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

			InstallStep6();
		}
		void InstallStep6()
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
				InstallStep4();
			}
			else
			{
				InstallStep7();
			}
		}
		void InstallStep7()
		{
			UpdateStatusAndFinish(true, "Successfully installed ReShade." + (isHeadless ? string.Empty : "\nClick the \"Finish\" button to exit the setup tool."));

			if (!isHeadless)
			{
				Dispatcher.Invoke(() =>
				{
					if (Keyboard.Modifiers != 0)
					{
						var dlg = new SettingsDialog(configPath) { Owner = this };

						dlg.ShowDialog();
					}
				});
			}
		}

		void UninstallStep0()
		{
			if (targetApi == Api.Vulkan)
			{
				string overrideMetaLayerPath = Path.Combine(commonPath, is64Bit ? "ReShade64_override.json" : "ReShade32_override.json");

				var overrideMetaLayerManifest = new JsonFile(overrideMetaLayerPath);

				overrideMetaLayerManifest.GetValue("layer.app_keys", out List<string> appKeys);
				appKeys.Remove(targetPath);
				overrideMetaLayerManifest.SetValue("layer.app_keys", appKeys);

				if (appKeys.Count == 0)
				{
					try
					{
						File.Delete(overrideMetaLayerPath);

						using (RegistryKey key = Registry.CurrentUser.CreateSubKey(Environment.Is64BitOperatingSystem && !is64Bit ? @"Software\Wow6432Node\Khronos\Vulkan\ImplicitLayers" : @"Software\Khronos\Vulkan\ImplicitLayers"))
						{
							key.DeleteValue(overrideMetaLayerPath);
						}
					}
					catch (Exception ex)
					{
						UpdateStatusAndFinish(false, "Failed to delete Vulkan meta layer manifest:\n" + ex.Message);
						return;
					}
				}
				else
				{
					overrideMetaLayerManifest.SaveFile();

					UninstallStep1();
					return;
				}
			}

			try
			{
				string basePath = Path.GetDirectoryName(configPath);

				File.Delete(modulePath);

				if (targetApi == Api.Vulkan && File.Exists(Path.ChangeExtension(modulePath, ".json")))
				{
					File.Delete(Path.ChangeExtension(modulePath, ".json"));
				}

				if (File.Exists(configPath))
				{
					File.Delete(configPath);
				}

				if (File.Exists(Path.ChangeExtension(modulePath, ".log")))
				{
					File.Delete(Path.ChangeExtension(modulePath, ".log"));
				}

				if (Directory.Exists(Path.Combine(basePath, "reshade-shaders")))
				{
					Directory.Delete(Path.Combine(basePath, "reshade-shaders"), true);
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

			if (CurrentPage.Content is SelectAppPage page0)
			{
				page0.Cancel();

				targetPath = page0.FileName;

				Task.Run(InstallStep0);
				return;
			}

			if (CurrentPage.Content is SelectApiPage page1)
			{
				if (page1.ApiD3D9.IsChecked == true)
				{
					targetApi = Api.D3D9;
				}
				if (page1.ApiDXGI.IsChecked == true)
				{
					targetApi = Api.DXGI;
				}
				if (page1.ApiOpenGL.IsChecked == true)
				{
					targetApi = Api.OpenGL;
				}
				if (page1.ApiVulkan.IsChecked == true)
				{
					targetApi = Api.Vulkan;
				}

				if (page1.AddonSupport.IsEnabled)
				{
					addonLoadSupport = page1.AddonSupport.IsChecked == true;
				}

				Task.Run(InstallStep2);
				return;
			}

			if (CurrentPage.Content is SelectPackagesPage page2)
			{
				packages = new Queue<EffectPackage>(page2.EnabledItems);

				if (packages.Count != 0)
				{
					Task.Run(InstallStep4);
				}
				else
				{
					Task.Run(InstallStep7);
				}
				return;
			}

			if (CurrentPage.Content is SelectEffectsPage page3)
			{
				Task.Run(() =>
				{
					// Delete all unselected effect files before moving
					foreach (string filePath in effects.Except(page3.EnabledItems.Select(x => x.FilePath)))
					{
						File.Delete(tempPathEffects + filePath.Remove(0, targetPathEffects.Length));
					}

					InstallStep6();
				});
				return;
			}
		}

		void OnCancelButtonClick(object sender, RoutedEventArgs e)
		{
			if (CurrentPage.Content is SelectAppPage page0)
			{
				page0.Cancel();
			}

			if (CurrentPage.Content is SelectPackagesPage page2)
			{
				Task.Run(InstallStep7);
				return;
			}

			if (CurrentPage.Content is SelectEffectsPage page3)
			{
				Task.Run(InstallStep6);
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
			CancelButton.Content = isFinished ? "_Back" : (e.Content is SelectPackagesPage || e.Content is SelectEffectsPage) ? "_Skip" : "_Cancel";

			CancelButton.IsEnabled = !(e.Content is StatusPage) || isFinished;

			if (!(e.Content is SelectAppPage))
			{
				NextButton.IsEnabled = isFinished || (!(e.Content is StatusPage) && !(e.Content is SelectUninstallPage));
			}
		}
	}
}
