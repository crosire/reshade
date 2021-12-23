/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System.Windows;
using System.Windows.Controls;

namespace ReShade.Setup.Pages
{
	public partial class StatusPage : Page
	{
		public StatusPage()
		{
			InitializeComponent();
		}

		public void UpdateStatus(string message, bool? status = null)
		{
			if (status is null)
			{
				StatusSpin.Visibility = Visibility.Visible;
				StatusSuccess.Visibility = Visibility.Collapsed;
				StatusFailure.Visibility = Visibility.Collapsed;
				SettingsButton.Visibility = Visibility.Collapsed;
			}
			else
			{
				StatusSpin.Visibility = Visibility.Collapsed;
				StatusSuccess.Visibility = status == true ? Visibility.Visible : Visibility.Collapsed;
				StatusFailure.Visibility = status == true ? Visibility.Collapsed : Visibility.Visible;
				SettingsButton.Visibility = status == true && !message.Contains("uninstall") ? Visibility.Visible : Visibility.Collapsed;
			}

			ProgressText.Text = message;
		}
	}
}
