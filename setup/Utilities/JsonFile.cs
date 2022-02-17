/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace ReShade.Setup.Utilities
{
	public class JsonFile
	{
		readonly string filePath;
		Dictionary<string, object> sections = new Dictionary<string, object>();

		public JsonFile(string path) : this(File.Exists(path) ? new FileStream(path, FileMode.Open) : null)
		{
			filePath = path;
		}
		public JsonFile(Stream stream)
		{
			if (stream == null)
			{
				return;
			}

			using (var reader = new StreamReader(stream, Encoding.UTF8))
			{
				ReadValue(reader, out object value);

				if (value is Dictionary<string, object> valueDictionary)
				{
					sections = valueDictionary;
				}
			}
		}

		void ReadValue(StreamReader reader, out object value)
		{
			SkipWhitespace(reader);

			var c = (char)reader.Read();

			if (c == '"')
			{
				ReadString(reader, out string valueString);
				value = valueString;
				return;
			}

			if (c == '[')
			{
				ReadListValue(reader, out List<object> valueList);
				value = valueList;
				return;
			}

			if (c == '{')
			{
				ReadDictionary(reader, out Dictionary<string, object> valueDictionary);
				value = valueDictionary;
				return;
			}

			value = null;
		}
		void ReadListValue(StreamReader reader, out List<object> value)
		{
			value = new List<object>();

			while (true)
			{
				SkipWhitespace(reader);

				var c = (char)reader.Read();
				if (c == ']')
				{
					break;
				}

				if (c == '"')
				{
					ReadString(reader, out string elementString);
					value.Add(elementString);

					SkipWhitespace(reader);
				}

				if (c == '{')
				{
					ReadDictionary(reader, out Dictionary<string, object> valueDictionary);
					value.Add(valueDictionary);

					SkipWhitespace(reader);
				}

				c = (char)reader.Peek();
				if (c == ',')
				{
					reader.Read();
				}
				else if (c != ']')
				{
					break;
				}
			}
		}
		void ReadDictionary(StreamReader reader, out Dictionary<string, object> value)
		{
			value = new Dictionary<string, object>();

			while (true)
			{
				SkipWhitespace(reader);

				var c = (char)reader.Read();
				if (c == '}')
				{
					break;
				}

				if (c == '"')
				{
					ReadString(reader, out string key);

					SkipWhitespace(reader);
					reader.Read();

					ReadValue(reader, out object element);
					value[key] = element;

					SkipWhitespace(reader);
				}

				c = (char)reader.Peek();
				if (c == ',')
				{
					reader.Read();
				}
				else if (c != '}')
				{
					break;
				}
			}
		}
		void ReadString(StreamReader reader, out string value)
		{
			var text = new StringBuilder();

			while (true)
			{
				var c = (char)reader.Read();
				if (c == '"')
				{
					break;
				}

				if (c == '\\')
				{
					char next = (char)reader.Read();
					switch (next)
					{
						case 'b':
							text.Append('\b');
							break;
						case 't':
							text.Append('\t');
							break;
						case 'n':
							text.Append('\n');
							break;
						case 'r':
							text.Append('\r');
							break;
						case '/':
						case '\\':
						case '"':
							text.Append(next);
							break;
						default:
							text.Append(c);
							text.Append(next);
							break;
					}
				}
				else
				{
					text.Append(c);
				}
			}

			value = text.ToString();
		}
		void SkipWhitespace(StreamReader reader)
		{
			while (true)
			{
				var c = (char)reader.Peek();
				if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
				{
					reader.Read();
				}
				else
				{
					break;
				}
			}
		}

		void WriteValue(StreamWriter writer, object value, int ident = 0)
		{
			if (value is string valueString)
			{
				WriteString(writer, valueString);
				return;
			}

			if (value is List<object> valueList)
			{
				WriteListValue(writer, valueList, ident);
				return;
			}

			if (value is Dictionary<string, object> valueDictionary)
			{
				WriteDictionary(writer, valueDictionary, ident);
				return;
			}
		}
		void WriteListValue(StreamWriter writer, List<object> value, int ident)
		{
			writer.WriteLine('[');

			int i = 0;

			foreach (object element in value)
			{
				writer.Write(new string(' ', (ident + 1) * 4));

				WriteValue(writer, element);

				if (++i == value.Count)
				{
					writer.WriteLine();
				}
				else
				{
					writer.WriteLine(',');
				}
			}

			if (ident != 0)
			{
				writer.Write(new string(' ', ident * 4));
			}

			writer.Write(']');
		}
		void WriteDictionary(StreamWriter writer, Dictionary<string, object> value, int ident)
		{
			writer.WriteLine('{');

			int i = 0;

			foreach (KeyValuePair<string, object> element in value)
			{
				writer.Write(new string(' ', (ident + 1) * 4));

				WriteString(writer, element.Key);

				writer.Write(": ");

				WriteValue(writer, element.Value, ident + 1);

				if (++i == value.Count)
				{
					writer.WriteLine();
				}
				else
				{
					writer.WriteLine(',');
				}
			}

			if (ident != 0)
			{
				writer.Write(new string(' ', ident * 4));
			}

			writer.Write('}');
		}
		void WriteString(StreamWriter writer, string value)
		{
			writer.Write('"');

			foreach (char c in value)
			{
				switch (c)
				{
					case '\b':
						writer.Write('\\');
						writer.Write('b');
						break;
					case '\t':
						writer.Write('\\');
						writer.Write('t');
						break;
					case '\n':
						writer.Write('\\');
						writer.Write('n');
						break;
					case '\r':
						writer.Write('\\');
						writer.Write('r');
						break;
					case '\\':
					case '"':
						writer.Write('\\');
						writer.Write(c);
						break;
					default:
						writer.Write(c);
						break;
				}
			}

			writer.Write('"');
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
			using (var stream = File.CreateText(path))
			{
				WriteValue(stream, sections);
				stream.WriteLine();
			}
		}

		public bool HasValue(string name)
		{
			var keys = name.Split('.');
			var section = sections;

			for (int i = 0; i < keys.Length && section != null; i++)
			{
				if (section.TryGetValue(keys[i], out object sectionValue))
				{
					if (i == (keys.Length - 1))
					{
						return true;
					}
					else
					{
						section = sectionValue as Dictionary<string, object>;
					}
				}
				else
				{
					break;
				}
			}

			return false;
		}

		public bool GetValue(string name, out List<string> value)
		{
			var keys = name.Split('.');
			var section = sections;

			for (int i = 0; i < keys.Length && section != null; i++)
			{
				if (section.TryGetValue(keys[i], out object sectionValue))
				{
					if (i == (keys.Length - 1))
					{
						if (sectionValue is List<object> sectionValueList)
						{
							value = sectionValueList.Cast<string>().ToList();
						}
						else
						{
							value = new List<string> { sectionValue.ToString() };
						}
						return true;
					}
					else
					{
						section = sectionValue as Dictionary<string, object>;
					}
				}
				else
				{
					break;
				}
			}

			value = new List<string>();
			return false;
		}
		public void SetValue(string name, List<string> value)
		{
			var keys = name.Split('.');
			var section = sections;

			for (int i = 0; i < keys.Length && section != null; i++)
			{
				if (i == (keys.Length - 1))
				{
					section[keys[i]] = value.Cast<object>().ToList();
					break;
				}

				if (section.TryGetValue(keys[i], out object sectionValue))
				{
					section = sectionValue as Dictionary<string, object>;
				}
				else
				{
					sectionValue = new Dictionary<string, object>();
					section[keys[i]] = sectionValue;
					section = sectionValue as Dictionary<string, object>;
				}
			}
		}
	}
}
