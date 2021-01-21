using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security;

namespace ReShade.Setup.Utilities
{
	public unsafe class PEInfo
	{
		public enum BinaryType : ushort
		{
			IMAGE_FILE_MACHINE_UNKNOWN = 0x0,
			IMAGE_FILE_MACHINE_I386 = 0x14c,
			IMAGE_FILE_MACHINE_AMD64 = 0x8664,
		}

		[StructLayout(LayoutKind.Sequential)]
		private struct LOADED_IMAGE
		{
			public IntPtr ModuleName;
			public IntPtr hFile;
			public IntPtr MappedAddress;
			public IntPtr FileHeader;
			public IntPtr LastRvaSection;
			public uint NumberOfSections;
			public IntPtr Sections;
			public uint Characteristics;
			public ushort fSystemImage;
			public ushort fDOSImage;
			public ushort fReadOnly;
			public ushort Version;
			public IntPtr Flink;
			public IntPtr BLink;
			public uint SizeOfImage;
		}
		[StructLayout(LayoutKind.Explicit)]
		private struct IMAGE_NT_HEADERS
		{
			[FieldOffset(0)]
			public uint Signature;
			[FieldOffset(4)]
			public IMAGE_FILE_HEADER FileHeader;
		}
		[StructLayout(LayoutKind.Sequential)]
		private struct IMAGE_FILE_HEADER
		{
			public BinaryType Machine;
			public ushort NumberOfSections;
			public uint TimeDateStamp;
			public uint PointerToSymbolTable;
			public uint NumberOfSymbols;
			public ushort SizeOfOptionalHeader;
			public ushort Characteristics;
		}
		[StructLayout(LayoutKind.Explicit)]
		private struct IMAGE_IMPORT_DESCRIPTOR
		{
			#region union
			[FieldOffset(0)]
			public uint Characteristics;
			[FieldOffset(0)]
			public uint OriginalFirstThunk;
			#endregion

			[FieldOffset(4)]
			public uint TimeDateStamp;
			[FieldOffset(8)]
			public uint ForwarderChain;
			[FieldOffset(12)]
			public uint Name;
			[FieldOffset(16)]
			public uint FirstThunk;
		}

		[DllImport("dbghelp.dll"), SuppressUnmanagedCodeSecurity]
		private static extern void* ImageDirectoryEntryToData(void* pBase, bool mappedAsImage, ushort directoryEntry, out uint size);
		[DllImport("dbghelp.dll"), SuppressUnmanagedCodeSecurity]
		private static extern IntPtr ImageRvaToVa(IntPtr pNtHeaders, IntPtr pBase, uint rva, IntPtr pLastRvaSection);
		[DllImport("imagehlp.dll"), SuppressUnmanagedCodeSecurity]
		private static extern bool MapAndLoad(string imageName, string dllPath, out LOADED_IMAGE loadedImage, bool dotDll, bool readOnly);

		// Adapted from http://stackoverflow.com/a/4696857/2055880
		public PEInfo(string path)
		{
			var modules = new List<string>();

			if (MapAndLoad(path, null, out LOADED_IMAGE image, true, true) && image.MappedAddress != IntPtr.Zero)
			{
				var imports = (IMAGE_IMPORT_DESCRIPTOR*)ImageDirectoryEntryToData((void*)image.MappedAddress, false, 1, out uint size);

				if (imports != null)
				{
					while (imports->OriginalFirstThunk != 0)
					{
						modules.Add(Marshal.PtrToStringAnsi(ImageRvaToVa(image.FileHeader, image.MappedAddress, imports->Name, IntPtr.Zero)));

						++imports;
					}
				}

				Type = ((IMAGE_NT_HEADERS*)image.FileHeader)->FileHeader.Machine;
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
	}
}
