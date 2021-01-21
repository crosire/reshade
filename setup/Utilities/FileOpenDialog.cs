using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace ReShade.Setup.Utilities
{
	public class FileOpenDialog
	{
		[Flags]
		enum FileOpenOptions : uint
		{
			FOS_ALLNONSTORAGEITEMS = 0x80,
			FOS_ALLOWMULTISELECT = 0x200,
			FOS_CREATEPROMPT = 0x2000,
			FOS_DEFAULTNOMINIMODE = 0x20000000,
			FOS_DONTADDTORECENT = 0x2000000,
			FOS_FILEMUSTEXIST = 0x1000,
			FOS_FORCEFILESYSTEM = 0x40,
			FOS_FORCESHOWHIDDEN = 0x10000000,
			FOS_HIDEMRUPLACES = 0x20000,
			FOS_HIDEPINNEDPLACES = 0x40000,
			FOS_NOCHANGEDIR = 8,
			FOS_NODEREFERENCELINKS = 0x100000,
			FOS_NOREADONLYRETURN = 0x8000,
			FOS_NOTESTFILECREATE = 0x10000,
			FOS_NOVALIDATE = 0x100,
			FOS_OVERWRITEPROMPT = 2,
			FOS_PATHMUSTEXIST = 0x800,
			FOS_PICKFOLDERS = 0x20,
			FOS_SHAREAWARE = 0x4000,
			FOS_STRICTFILETYPES = 4
		}

		enum ShellItemDesignNameOptions : uint
		{
			SIGDN_DESKTOPABSOLUTEEDITING = 0x8004c000,
			SIGDN_DESKTOPABSOLUTEPARSING = 0x80028000,
			SIGDN_FILESYSPATH = 0x80058000,
			SIGDN_NORMALDISPLAY = 0,
			SIGDN_PARENTRELATIVE = 0x80080001,
			SIGDN_PARENTRELATIVEEDITING = 0x80031001,
			SIGDN_PARENTRELATIVEFORADDRESSBAR = 0x8007c001,
			SIGDN_PARENTRELATIVEPARSING = 0x80018001,
			SIGDN_URL = 0x80068000
		}

		[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
		internal struct FilterSpec
		{
			[MarshalAs(UnmanagedType.LPWStr)]
			internal string Name;
			[MarshalAs(UnmanagedType.LPWStr)]
			internal string Spec;
		}


		[ComImport, Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE")]
		[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
		interface IShellItem
		{
			void BindToHandler(); // not fully defined
			void GetParent(out IShellItem ppsi);
			void GetDisplayName(ShellItemDesignNameOptions sigdnName, [MarshalAs(UnmanagedType.LPWStr)] out string ppszName);
			void GetAttributes(uint sfgaoMask, out uint psfgaoAttribs);
			void Compare(IShellItem psi, int hint, out int piOrder);
		}

		[ComImport, Guid("B63EA76D-1F85-456F-A19C-48159EFA858B")]
		[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
		interface IShellItemArray
		{
			void BindToHandler([In, MarshalAs(UnmanagedType.Interface)] IntPtr pbc, [In] ref Guid rbhid, [In] ref Guid riid, out IntPtr ppvOut);
			void GetPropertyStore(int Flags, [In] ref Guid riid, out IntPtr ppv);
			void GetPropertyDescriptionList([In] IntPtr keyType, [In] ref Guid riid, out IntPtr ppv);
			void GetAttributes(uint dwAttribFlags, uint sfgaoMask, out uint psfgaoAttribs);
			void GetCount(out uint pdwNumItems);
			void GetItemAt(uint dwIndex, out IShellItem ppsi);
			void EnumItems(out IntPtr ppenumShellItems);
		}

		[ComImport, Guid("D57C7288-D4AD-4768-BE02-9D969532D960")]
		[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
		interface IFileOpenDialog
		{
			void Show([In] IntPtr parent);
			void SetFileTypes(uint cFileTypes, [In, MarshalAs(UnmanagedType.LPArray)] FilterSpec[] rgFilterSpec);
			void SetFileTypeIndex(uint iFileType);
			void GetFileTypeIndex(out uint piFileType);
			void Advise([In] IntPtr pfde, out uint pdwCookie);
			void Unadvise(uint dwCookie);
			void SetOptions(FileOpenOptions fos);
			void GetOptions(out FileOpenOptions pfos);
			void SetDefaultFolder(IShellItem psi);
			void SetFolder(IShellItem psi);
			void GetFolder(out IShellItem ppsi);
			void GetCurrentSelection(out IShellItem ppsi);
			void SetFileName([In, MarshalAs(UnmanagedType.LPWStr)] string pszName);
			void GetFileName([MarshalAs(UnmanagedType.LPWStr)] out string pszName);
			void SetTitle([In, MarshalAs(UnmanagedType.LPWStr)] string pszTitle);
			void SetOkButtonLabel([In, MarshalAs(UnmanagedType.LPWStr)] string pszText);
			void SetFileNameLabel([In, MarshalAs(UnmanagedType.LPWStr)] string pszLabel);
			void GetResult(out IShellItem ppsi);
			void AddPlace(IShellItem psi, int fdap);
			void SetDefaultExtension([In, MarshalAs(UnmanagedType.LPWStr)] string pszDefaultExtension);
			void Close([MarshalAs(UnmanagedType.Error)] int hr);
			void SetClientGuid([In] ref Guid guid);
			void ClearClientData();
			void SetFilter([In, MarshalAs(UnmanagedType.Interface)] IntPtr pFilter);
			void GetResults(out IShellItemArray ppenum);
			void GetSelectedItems(out IShellItemArray ppsai);
		}

		[ComImport, Guid("DC1C5A9C-E88A-4dde-A5A1-60F82A20AEF7")]
		class FileOpenDialogInternal
		{
		}

		[DllImport("shell32.dll")]
		static extern void SHCreateShellItem(IntPtr pidlParent, IntPtr psfParent, IntPtr pidl, out IShellItem ppsi);
		[DllImport("shell32.dll")]
		static extern void SHILCreateFromPath([MarshalAs(UnmanagedType.LPWStr)] string pszPath, out IntPtr ppIdl, ref uint rgflnOut);

		public bool Multiselect
		{
			set
			{
				if (value)
					options |= FileOpenOptions.FOS_ALLOWMULTISELECT;
				else
					options &= ~FileOpenOptions.FOS_ALLOWMULTISELECT;
			}
		}

		public bool FolderPicker
		{
			set
			{
				if (value)
					options |= FileOpenOptions.FOS_PICKFOLDERS;
				else
					options &= ~FileOpenOptions.FOS_PICKFOLDERS;
			}
		}

		public bool CheckPathExists
		{
			set
			{
				if (value)
					options |= FileOpenOptions.FOS_PATHMUSTEXIST;
				else
					options &= ~FileOpenOptions.FOS_PATHMUSTEXIST;
			}
		}

		public bool CheckFileExists
		{
			set
			{
				if (value)
					options |= FileOpenOptions.FOS_FILEMUSTEXIST;
				else
					options &= ~FileOpenOptions.FOS_FILEMUSTEXIST;
			}
		}

		public string Filter
		{
			set
			{
				var filter = value.Split('|');
				var filterSpec = new FilterSpec { Name = filter[0], Spec = filter[1] };
				dialog.SetFileTypes(1, new FilterSpec[] { filterSpec });
			}
		}

		public string FileName
		{
			get
			{
				var fileNames = FileNames;
				return fileNames.Length != 0 ? fileNames[0] : null;
			}
			set
			{
				dialog.SetFileName(value);
			}
		}

		public string DefaultExt
		{
			set
			{
				dialog.SetDefaultExtension(value);
			}
		}

		public string InitialDirectory
		{
			set
			{
				uint attributes = 0;
				SHILCreateFromPath(value, out IntPtr idl, ref attributes);
				SHCreateShellItem(IntPtr.Zero, IntPtr.Zero, idl, out IShellItem item);
				dialog.SetFolder(item);
				Marshal.FreeCoTaskMem(idl);
			}
		}

		public string[] FileNames
		{
			get
			{
				dialog.GetResults(out IShellItemArray results);

				results.GetCount(out uint num);
				var result = new string[num];
				for (uint i = 0; i < num; ++i)
				{
					results.GetItemAt(i, out IShellItem shellitem);
					shellitem.GetDisplayName(ShellItemDesignNameOptions.SIGDN_FILESYSPATH, out string path);

					result[i] = path;
				}

				return result;
			}
		}

		FileOpenOptions options = FileOpenOptions.FOS_FORCEFILESYSTEM;
		readonly IFileOpenDialog dialog = (IFileOpenDialog)new FileOpenDialogInternal();

		/// <summary>
		/// Displays a file/folder selection dialog.
		/// </summary>
		/// <param name="owner">The parent window.</param>
		public bool? ShowDialog(Window owner)
		{
			try
			{
				dialog.SetOptions(options);
				dialog.Show(new WindowInteropHelper(owner).Handle);

				return true;
			}
			catch (Exception ex)
			{
				return ex.HResult == unchecked((int)0x800704C7) ? (bool?)false : null;
			}
		}
	}
}
