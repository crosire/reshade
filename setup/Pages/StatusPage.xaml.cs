/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

using System.Diagnostics;
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
				PatreonButton.Visibility = Visibility.Collapsed;
			}
			else
			{
				StatusSpin.Visibility = Visibility.Collapsed;
				StatusSuccess.Visibility = status == true ? Visibility.Visible : Visibility.Collapsed;
				StatusFailure.Visibility = status == true ? Visibility.Collapsed : Visibility.Visible;
				PatreonButton.Visibility = status == true ? Visibility.Visible : Visibility.Collapsed;
			}

			ProgressText.Text = message;
		}

		private void OnPatreonButtonClick(object sender, RoutedEventArgs e)
		{
			Process.Start("https://patreon.com/crosire");
		}
	}
}
