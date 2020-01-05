using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace ReShade.Utilities
{
	public class IniFile
	{
		readonly string filePath;
		SortedDictionary<string, SortedDictionary<string, string[]>> sections =
			new SortedDictionary<string, SortedDictionary<string, string[]>>();

		public IniFile(string path)
		{
			filePath = path;

			if (!File.Exists(filePath))
			{
				return;
			}

			var section = string.Empty;

			foreach (var next in File.ReadLines(filePath, Encoding.UTF8))
			{
				var line = next.Trim();
				if (string.IsNullOrEmpty(line) ||
					// Ignore lines which are comments
					line.StartsWith(";", StringComparison.Ordinal) ||
					line.StartsWith("//", StringComparison.Ordinal))
				{
					continue;
				}

				if (line.StartsWith("[", StringComparison.Ordinal) && line.EndsWith("]", StringComparison.Ordinal))
				{
					// This is a section definition
					section = line.Substring(1, line.Length - 2);
					continue;
				}

				var pair = line.Split(new[] { '=' }, 2, StringSplitOptions.None);
				if (pair.Length == 2 && pair[0].Trim() is var key && pair[1].Trim() is var value)
				{
					SetValue(section, key, value.Split(new[] { ',' }, StringSplitOptions.None));
				}
				else
				{
					SetValue(section, line);
				}
			}
		}

		public void Save()
		{
			var text = new StringBuilder();

			foreach (var section in sections)
			{
				if (!string.IsNullOrEmpty(section.Key))
				{
					text.AppendLine("[" + section.Key + "]");
				}

				foreach (var pair in section.Value)
				{
					text.AppendLine(pair.Key + "=" + string.Join(",", pair.Value));
				}

				text.AppendLine();
			}

			text.AppendLine();

			File.WriteAllText(filePath, text.ToString(), Encoding.UTF8);
		}

		public bool GetValue(string section, string key, out string[] value)
		{
			if (!sections.TryGetValue(section, out var sectionData))
			{
				value = default;
				return false;
			}

			return sectionData.TryGetValue(key, out value);
		}
		public void SetValue(string section, string key, params string[] value)
		{
			if (!sections.TryGetValue(section, out var sectionData))
			{
				sections[section] = sectionData = new SortedDictionary<string, string[]>();
			}

			sectionData[key] = value ?? new string[] { };
		}

		public string GetString(string section, string key, string def = default)
		{
			return GetValue(section, key, out var value) ? string.Join(",", value) : def;
		}
	}
}
