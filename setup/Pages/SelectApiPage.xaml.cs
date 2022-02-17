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

		private void OnHyperlinkNavigate(object sender, RequestNavigateEventArgs e)
		{
			Process.Start(e.Uri.ToString());
			e.Handled = true;
		}
	}
}
