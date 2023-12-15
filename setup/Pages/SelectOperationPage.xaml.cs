/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Windows.Controls;

namespace ReShade.Setup.Pages
{
	public enum InstallOperation
	{
		Install,
		Update,
		UpdateWithEffects,
		Uninstall,
		Finished
	}

	public partial class SelectOperationPage : Page
	{
		public SelectOperationPage()
		{
			InitializeComponent();
		}

		public InstallOperation SelectedOperation
		{
			get
			{
				if (UpdateButton.IsChecked == true)
				{
					return InstallOperation.Update;
				}
				if (ModifyButton.IsChecked == true)
				{
					return InstallOperation.UpdateWithEffects;
				}
				if (UninstallButton.IsChecked == true)
				{
					return InstallOperation.Uninstall;
				}

				return InstallOperation.Install;
			}
			set
			{
				UpdateButton.IsChecked = value == InstallOperation.Update;
				ModifyButton.IsChecked = value == InstallOperation.UpdateWithEffects;
				UninstallButton.IsChecked = value == InstallOperation.Uninstall;
			}
		}
	}
}
