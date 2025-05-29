using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace ReShade.Setup.Utilities
{
	public unsafe class PEInfo
	{
		public enum BinaryType : UInt16
		{
			IMAGE_FILE_MACHINE_UNKNOWN = 0x0,
			IMAGE_FILE_MACHINE_I386 = 0x14c,
			IMAGE_FILE_MACHINE_AMD64 = 0x8664,
		}

		public enum ImageDirectory : UInt16
		{
			IMAGE_DIRECTORY_ENTRY_EXPORT = 0,
			IMAGE_DIRECTORY_ENTRY_IMPORT = 1,
		}

		[StructLayout(LayoutKind.Sequential)]
		struct LOADED_IMAGE
		{
			public IntPtr ModuleName;
			public IntPtr hFile;
			public IntPtr MappedAddress;
			public IntPtr FileHeader;
			public IntPtr LastRvaSection;
			public UInt32 NumberOfSections;
			public IntPtr Sections;
			public UInt32 Characteristics;
			public UInt16 fSystemImage;
			public UInt16 fDOSImage;
			public UInt16 fReadOnly;
			public UInt16 Version;
			public IntPtr Flink;
			public IntPtr BLink;
			public UInt32 SizeOfImage;
		}

		[StructLayout(LayoutKind.Sequential)]
		struct IMAGE_NT_HEADERS
		{
			public UInt32 Signature;
			public IMAGE_FILE_HEADER FileHeader;
		}

		[StructLayout(LayoutKind.Sequential)]
		struct IMAGE_FILE_HEADER
		{
			public BinaryType Machine;
			public UInt16 NumberOfSections;
			public UInt32 TimeDateStamp;
			public UInt32 PointerToSymbolTable;
			public UInt32 NumberOfSymbols;
			public UInt16 SizeOfOptionalHeader;
			public UInt16 Characteristics;
		}

		[StructLayout(LayoutKind.Explicit)]
		struct IMAGE_IMPORT_DESCRIPTOR
		{
			[FieldOffset(0)]
			public UInt32 Characteristics;
			[FieldOffset(0)]
			public UInt32 OriginalFirstThunk;
			[FieldOffset(4)]
			public UInt32 TimeDateStamp;
			[FieldOffset(8)]
			public UInt32 ForwarderChain;
			[FieldOffset(12)]
			public UInt32 Name;
			[FieldOffset(16)]
			public UInt32 FirstThunk;
		}

		[DllImport("imagehlp.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		static extern bool MapAndLoad([In, MarshalAs(UnmanagedType.LPStr)] string imageName, [In, MarshalAs(UnmanagedType.LPStr)] string dllPath, [Out] out LOADED_IMAGE loadedImage, [In, MarshalAs(UnmanagedType.Bool)] bool dotDll, [In, MarshalAs(UnmanagedType.Bool)] bool readOnly);
		[DllImport("imagehlp.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		static extern bool UnMapAndLoad([In] ref LOADED_IMAGE loadedImage);

		[DllImport("dbghelp.dll", SetLastError = true)]
		static extern IntPtr ImageRvaToVa([In] IntPtr pNtHeaders, [In] IntPtr pBase, [In] uint rva, [In] IntPtr pLastRvaSection);
		[DllImport("dbghelp.dll", SetLastError = true)]
		static extern IntPtr ImageDirectoryEntryToData([In] IntPtr pBase, [In, MarshalAs(UnmanagedType.U1)] bool mappedAsImage, [In] ImageDirectory directoryEntry, [Out] out uint size);

		[Flags]
		public enum LoadLibraryFlags : UInt32
		{
			LOAD_LIBRARY_AS_DATAFILE = 0x00000002
		}

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern IntPtr LoadLibraryEx([In] string lpFileName, [In] IntPtr hFile, [In] LoadLibraryFlags dwFlags);
		[DllImport("kernel32.dll", SetLastError = true)]
		[return: MarshalAs(UnmanagedType.Bool)]
		static extern bool FreeLibrary([In] IntPtr hModule);

		[DllImport("kernel32.dll", SetLastError = true)]
		static extern IntPtr FindResource([In] IntPtr hModule, [In] string lpName, [In] string lpType);
		[DllImport("kernel32.dll", SetLastError = true)]
		static extern IntPtr LoadResource([In] IntPtr hModule, [In] IntPtr hResInfo);
		[DllImport("kernel32.dll", SetLastError = true)]
		static extern IntPtr LockResource([In] IntPtr hResData);
		[DllImport("kernel32.dll", SetLastError = true)]
		static extern UInt32 SizeofResource([In] IntPtr hModule, [In] IntPtr hResInfo);

		// Adapted from http://stackoverflow.com/a/4696857/2055880
		public PEInfo(string path)
		{
			var modules = new List<string>();

			if (MapAndLoad(path, null, out LOADED_IMAGE image, false, true))
			{
				var imports = (IMAGE_IMPORT_DESCRIPTOR*)ImageDirectoryEntryToData(image.MappedAddress, false, ImageDirectory.IMAGE_DIRECTORY_ENTRY_IMPORT, out uint size);

				if (imports != null)
				{
					while (imports->OriginalFirstThunk != 0)
					{
						string module = Marshal.PtrToStringAnsi(ImageRvaToVa(image.FileHeader, image.MappedAddress, imports->Name, IntPtr.Zero));
						if (!string.IsNullOrEmpty(module))
						{
							modules.Add(module);
						}

						++imports;
					}
				}

				Type = ((IMAGE_NT_HEADERS*)image.FileHeader)->FileHeader.Machine;

				UnMapAndLoad(ref image);
			}

			Modules = modules;
		}

		public BinaryType Type
		{
			get;
		}

		public IEnumerable<string> Modules
		{
			get;
		}

		public static string ReadResourceString(string path, ushort id)
		{
			string result = null;

			IntPtr module = LoadLibraryEx(path, IntPtr.Zero, LoadLibraryFlags.LOAD_LIBRARY_AS_DATAFILE);
			if (module != IntPtr.Zero)
			{
				IntPtr info = FindResource(module, "#" + id.ToString(), "#10" /* RCDATA */);
				if (info != IntPtr.Zero)
				{
					IntPtr handle = LoadResource(module, info);
					if (handle != IntPtr.Zero)
					{
						IntPtr data = LockResource(handle);
						UInt32 dataSize = SizeofResource(module, info);

						result = Marshal.PtrToStringUni(data, (int)(dataSize / 2) - 1);
					}
				}

				FreeLibrary(module);
			}

			return result;
		}
	}
}
