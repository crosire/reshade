using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class IniFile
{
	private string m_file;
	private SortedDictionary<string, SortedDictionary<string, string[]>> m_sections = new SortedDictionary<string, SortedDictionary<string, string[]>> { };

	public IniFile(string path)
	{
		m_file = path;
		Load();
	}

	private void Load()
	{
		if (!File.Exists(m_file))
			return;

		var sections = new SortedDictionary<string, SortedDictionary<string, string[]>> { };
		var section = "";

		foreach (var next in File.ReadLines(m_file, Encoding.UTF8))
		{
			var line = next.Trim();
			if (string.IsNullOrEmpty(line) || line.StartsWith(";", StringComparison.Ordinal) || line.StartsWith("/", StringComparison.Ordinal))
			{
				continue;
			}
			if (line.StartsWith("[", StringComparison.Ordinal) && line.EndsWith("]", StringComparison.Ordinal))
			{
				section = line.Substring(1, line.Length - 2);
				continue;
			}
			if (!sections.TryGetValue(section, out var pairs))
			{
				sections[section] = pairs = new SortedDictionary<string, string[]> { };
			}
			var pair = line.Split(new[] { '=' }, 2, StringSplitOptions.None);
			if (pair.Length == 2 && pair[0].Trim() is var key && pair[1].Trim() is var value)
			{
				pairs[key] = value.Split(new[] { ',' }, StringSplitOptions.None);
			}
			else
			{
				pairs[line] = new string[] { };
			}
		}

		m_sections = sections;
	}

	public void Save()
	{
		var text = new StringBuilder { };
		foreach (var section in m_sections)
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
		File.WriteAllText(m_file, text.ToString(), Encoding.UTF8);
	}

	public string GetValue(string section, string key, string def = default) => TryGetValue(section, key, out var value) ? string.Join(",", value) : def;
	public bool TryGetValue(string section, string key, out string[] value)
	{
		if (m_sections.TryGetValue(section, out var pair))
		{
			return pair.TryGetValue(key, out value);
		}
		value = default;
		return false;
	}

	public void SetValue(string section, string key, params string[] value)
	{
		if (!m_sections.TryGetValue(section, out var pairs))
		{
			m_sections[section] = pairs = new SortedDictionary<string, string[]> { };
		}
		pairs[key] = value ?? new string[] { };
	}
}
