/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Windows.Controls;

namespace ReShade.Setup.Pages
{
	public enum InstallOperation
	{
		Default,
		Update,
		Modify,
		Uninstall,
		Finished
	}

	public partial class SelectUninstallPage : Page
	{
		public SelectUninstallPage()
		{
			InitializeComponent();
		}
	}
}
