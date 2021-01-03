/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace ReShade.Setup.Utilities
{
	public class IniFile
	{
		readonly string filePath;
		SortedDictionary<string, SortedDictionary<string, string[]>> sections =
			new SortedDictionary<string, SortedDictionary<string, string[]>>();

		public IniFile(string path) : this(File.Exists(path) ? new FileStream(path, FileMode.Open) : null)
		{
			filePath = path;
		}
		public IniFile(Stream stream)
		{
			if (stream == null)
			{
				return;
			}

			var section = string.Empty;

			using (var reader = new StreamReader(stream, Encoding.UTF8))
			{
				while (!reader.EndOfStream)
				{
					string line = reader.ReadLine().Trim();
					if (string.IsNullOrEmpty(line) ||
						// Ignore lines which are comments
						line.StartsWith(";", StringComparison.Ordinal) ||
						line.StartsWith("#", StringComparison.Ordinal) ||
						line.StartsWith("//", StringComparison.Ordinal))
					{
						continue;
					}

					if (line.StartsWith("[", StringComparison.Ordinal))
					{
						int sectionEnd = line.IndexOf(']');
						if (sectionEnd >= 0)
						{
							// This is a section definition
							section = line.Substring(1, sectionEnd - 1);
							continue;
						}
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
		}

		public void SaveFile()
		{
			if (filePath == null)
			{
				throw new InvalidOperationException();
			}

			SaveFile(filePath);
		}
		public void SaveFile(string path)
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

			File.WriteAllText(path, text.ToString(), Encoding.UTF8);
		}

		public bool HasValue(string section)
		{
			return sections.ContainsKey(section);
		}
		public bool HasValue(string section, string key)
		{
			return sections.TryGetValue(section, out var sectionData) && sectionData.ContainsKey(key);
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
				if (value == null)
				{
					// Do not add null value if it does not exist yet
					return;
				}

				sections[section] = sectionData = new SortedDictionary<string, string[]>();
			}

			sectionData[key] = value ?? new string[] { };
		}

		public void RenameValue(string section, string key, string newKey)
		{
			var value = GetString(section, key);
			if (value != null)
			{
				SetValue(section, newKey, value);
				RemoveValue(section, key);
			}
		}
		public void RenameValue(string section, string key, string newSection, string newKey)
		{
			var value = GetString(section, key);
			if (value != null)
			{
				SetValue(newSection, newKey, value);
				RemoveValue(section, key);
			}
		}

		public void RemoveValue(string section, string key)
		{
			if (sections.TryGetValue(section, out var sectionData))
			{
				sectionData.Remove(key);

				if (sectionData.Count == 0)
				{
					sections.Remove(section);
				}
			}
		}

		public string GetString(string section, string key, string defaultValue = null)
		{
			return GetValue(section, key, out var value) ? string.Join(",", value) : defaultValue;
		}

		public string[] GetSections()
		{
			return sections.Select(x => x.Key).ToArray();
		}
	}
}
