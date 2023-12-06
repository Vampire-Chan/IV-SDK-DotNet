#pragma once

public ref class Logger
{
public:

	value struct tLogItem
	{
	public:

		/// <summary>
		/// Gets the time this item was logged.
		/// </summary>
		property DateTime LogTime
		{
		public:
			DateTime get()
			{
				return m_LogTime;
			}
		}

		/// <summary>
		/// Gets the style of this item for the IV-SDK .NET Console.
		/// </summary>
		property IVSDKDotNet::Enums::eConsoleLogStyle Style
		{
		public:
			IVSDKDotNet::Enums::eConsoleLogStyle get()
			{
				return m_Style;
			}
		}

		/// <summary>
		/// Get the text of this item.
		/// </summary>
		property String^ Text
		{
		public:
			String^ get()
			{
				return m_Text;
			}
		}

		/// <summary>
		/// Gets if this item should also be in the IV-SDK .NET Console.
		/// </summary>
		property bool AddInConsole
		{
		public:
			bool get()
			{
				return m_AddInConsole;
			}
		}

		virtual String^ ToString() override
		{
			return String::Format("[{0}] {1}", LogTime, Text);
		}

	internal:
		tLogItem(DateTime logTime, IVSDKDotNet::Enums::eConsoleLogStyle style, String^ text, bool addInConsole)
		{
			m_LogTime = logTime;
			m_Style = style;
			m_Text = text;
			m_AddInConsole = addInConsole;
		}

	private:
		DateTime m_LogTime;
		IVSDKDotNet::Enums::eConsoleLogStyle m_Style;
		String^ m_Text;
		bool m_AddInConsole;
	};

	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console (if set) without a specific style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	/// <param name="alsoShowInConsole">Sets if it should also show up in the IV-SDK .NET Console.</param>
	static void Log(String^ str, bool alsoShowInConsole);
	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console without a specific style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	static void Log(String^ str);

	/// <summary>
	/// Only available in debug builds! Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console (if set) with the "Debug" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	/// <param name="alsoShowInConsole">Sets if it should also show up in the IV-SDK .NET Console.</param>
	static void LogDebug(String^ str, bool alsoShowInConsole);
	/// <summary>
	/// Only available in debug builds! Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console with the "Debug" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	static void LogDebug(String^ str);

	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console (if set) with the "Warning" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	/// <param name="alsoShowInConsole">Sets if it should also show up in the IV-SDK .NET Console.</param>
	static void LogWarning(String^ str, bool alsoShowInConsole);
	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console with the "Warning" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	static void LogWarning(String^ str);

	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console (if set) with the "Error" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	/// <param name="alsoShowInConsole">Sets if it should also show up in the IV-SDK .NET Console.</param>
	static void LogError(String^ str, bool alsoShowInConsole);
	/// <summary>
	/// Logs a message to the IVSDKDotNet.log file and also shows it in the IV-SDK .NET Console with the "Error" style.
	/// </summary>
	/// <param name="str">The message to log.</param>
	static void LogError(String^ str);

	static void ClearLogItems();
	static List<tLogItem>^ GetLogItems();
	static array<String^>^ GetLogItemsAsString();

internal:
	static void Initialize();

private:
	static const int m_MaxTextLengthUntilSplit = 140;
	static array<String^>^ m_SplitStr = gcnew array<String^>(1) { Environment::NewLine };
	static List<tLogItem>^ m_LogItems;

	static void Log(IVSDKDotNet::Enums::eConsoleLogStyle style, String^ str, bool alsoShowInConsole);
};