/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Diagnostics;
using System.Windows.Controls;
using System.Windows.Navigation;

namespace ReShade.Setup.Pages
{
	public enum Api
	{
		Unknown,
		D3D9,
		D3D10,
		D3D11,
		D3D12,
		DXGI,
		OpenGL,
		Vulkan
	}

	public partial class SelectApiPage : Page
	{
		public SelectApiPage(string appName)
		{
			InitializeComponent();

			AppName.Text = appName;
		}

		public Api SelectedApi
		{
			get
			{
				if (ApiD3D9.IsChecked == true)
				{
					return Api.D3D9;
				}
				if (ApiDXGI.IsChecked == true)
				{
					return Api.DXGI;
				}
				if (ApiOpenGL.IsChecked == true)
				{
					return Api.OpenGL;
				}
				if (ApiVulkan.IsChecked == true)
				{
					return Api.Vulkan;
				}

				return Api.Unknown;
			}
			set
			{
				ApiD3D9.IsChecked = value == Api.D3D9;
				ApiDXGI.IsChecked = value == Api.D3D10 || value == Api.D3D11 || value == Api.D3D12 || value == Api.DXGI;
				ApiOpenGL.IsChecked = value == Api.OpenGL;
				ApiVulkan.IsChecked = value == Api.Vulkan;
			}
		}
		public bool SelectedOpenXR
		{
			get => ApiOpenXR.IsChecked == true;
			set => ApiOpenXR.IsChecked = value;
		}

		private void OnHyperlinkNavigate(object sender, RequestNavigateEventArgs e)
		{
			Process.Start(e.Uri.ToString());
			e.Handled = true;
		}
	}
}
