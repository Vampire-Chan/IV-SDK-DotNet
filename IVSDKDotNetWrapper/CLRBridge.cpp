#include "pch.h"
#include "CLRBridge.h"

using namespace IVSDKDotNet;
using namespace IVSDKDotNet::Manager;

WNDPROC originalWndProc = nullptr;
LRESULT __stdcall WndProcHook(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Window starts being destroyed
	if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) // WM_DESTROY
	{
		// Invoke event for scripts
		RAGE::RaiseOnExit();

		// Cleanup internal things
		CLR::CLRBridge::Cleanup();

		// Wait for cleanup process to finish
		while (!CLR::CLRBridge::CanTerminate)
		{
			Sleep(1);
		}

		// Call the original function
		return CallWindowProc(originalWndProc, hWnd, uMsg, wParam, lParam);
	}

	// Invoke event
	RAGE::RaiseOnWndProcMessageReceived(IntPtr(hWnd), uMsg, UIntPtr(wParam), IntPtr(lParam));

	// Invoke ImGui WndProc
	if (ImGuiDraw::OnWndProc(hWnd, uMsg, wParam, lParam))
		return 1L;

	// Call the original function
	return CallWindowProc(originalWndProc, hWnd, uMsg, wParam, lParam);
}

namespace CLR
{

	void CLRBridge::Initialize(uint32_t version, uint32_t baseAddress)
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		// Set paths
		IVSDKDotNetPath =			Application::StartupPath + "\\IVSDKDotNet";
		IVSDKDotNetBinaryPath =		IVSDKDotNetPath + "\\bin";
		IVSDKDotNetDataPath =		IVSDKDotNetPath + "\\data";
		IVSDKDotNetLogsPath =		IVSDKDotNetPath + "\\logs";
		IVSDKDotNetPluginsPath =	IVSDKDotNetPath + "\\plugins";
		IVSDKDotNetScriptsPath =	IVSDKDotNetPath + "\\scripts";
		m_sIVSDKDotNetManagerPath = IVSDKDotNetPath + "\\IVSDKDotNet.Manager.dll";

		// Set current log file name
		DateTime dtNow = DateTime::Now;
		CurrentLogFileName = String::Format("IVSDKDotNet@{0}{1}{2}@{3}.log", dtNow.Day, dtNow.Month, dtNow.Year, dtNow.Millisecond);

		// Initialize and try loading the IVSDKDotNet settings file
		Settings = gcnew SettingsFile(IVSDKDotNetPath + "\\config.ini");
		bool settingsLoadResult = Settings->Load();
		
		// Initialize logger
		Logger::Initialize();

		// Force english culture of current thread
		System::Globalization::CultureInfo^ cultureInfo = gcnew System::Globalization::CultureInfo("en-US");
		System::Threading::Thread::CurrentThread->CurrentCulture = cultureInfo;
		System::Threading::Thread::CurrentThread->CurrentUICulture = cultureInfo;

		// Print about text to console
#if _DEBUG
		Logger::Log(String::Format("IV-SDK .NET DEBUG version {0} by ItsClonkAndre", Version));

		// Launch debugger
		if (Settings->GetBoolean("DEBUG", "LaunchDebugger", false))
			Debugger::Launch();
#elif PREVIEW
		Logger::Log(String::Format("IV-SDK .NET PREVIEW version {0} by ItsClonkAndre", Version));
#else
		Logger::Log(String::Format("IV-SDK .NET Release version {0} by ItsClonkAndre", Version));
#endif

		// Log settings file load result
		if (settingsLoadResult)
			Logger::Log("Loaded IV-SDK .NET config.ini!");
		else
			Logger::LogWarning("Could not load IV-SDK .NET config.ini! Using default settings.");

		// Do early checks
		if (!DoEarlyChecks())
		{
			LetUserKnowAboutFailedInitialization();
			return;
		}

		// Initialize Memory Access stuff
		MemoryAccess::Initialise(version, baseAddress);

		// Get the GTA IV Process
		TheGTAProcess = Process::GetCurrentProcess();

		// Wait till process has a MainWindowHandle and for g_pDirect3DDevice to return a valid pointer
		while (TheGTAProcess->MainWindowHandle == IntPtr::Zero)
			Sleep(100);
		while (rage::g_pDirect3DDevice == nullptr)
			Sleep(100);

		// Initialize stuff
		MH_STATUS minHookStatus = MH_Initialize();

		if (minHookStatus != MH_STATUS::MH_OK)
		{
			Logger::LogError(String::Format("[MinHook] Failed to initialize! Details: {0}", gcnew String(MH_StatusToString(minHookStatus))));
			LetUserKnowAboutFailedInitialization();
			return;
		}
		else if (minHookStatus == MH_STATUS::MH_OK)
		{
			m_bHasMinHookInitialized = true;
		}

		// Hook stuff
		if (DXHook::Initialize())
			Logger::LogDebug("[DXHook] Done!");
		if (DirectInputHook::Initialize())
			Logger::LogDebug("[DirectInputHook] Done!");
		if (XInputHook::Initialize())
			Logger::LogDebug("[XInputHook] Done!");
		if (UnmanagedGameHooks::Initialize())
			Logger::LogDebug("[GameHooks] Done!");

		// FindWindow(L"grcWindow", L"GTAIV")
		originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr((HWND)TheGTAProcess->MainWindowHandle.ToPointer(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProcHook)));

		// Enable hooks
		minHookStatus = MH_EnableHook(MH_ALL_HOOKS);

		if (minHookStatus != MH_STATUS::MH_OK)
		{
			Logger::LogError(String::Format("[MinHook] Failed to enable Hooks! Details: {0}", gcnew String(MH_StatusToString(minHookStatus))));
			LetUserKnowAboutFailedInitialization();
			return;
		}

		// Do additional checks
		DoAdditionalChecks();

		// Do early assembly loading
		if (!DoEarlyAdditionalAssemblyLoading())
		{
			LetUserKnowAboutFailedInitialization();
			return;
		}

		// Load and initialize the manager
		if (!InitManager())
		{
			LetUserKnowAboutFailedInitialization();
			return;
		}
	
		// Schleep a bit
		Sleep(1100);

		// Set drawing allowed now
		ImGuiIV::CanDraw = true;

		// Load scripts
		LoadScripts();

		//// Do additional checks
		//DoAdditionalChecks();

		//// Do early assembly loading
		//if (!DoEarlyAdditionalAssemblyLoading())
		//{
		//	LetUserKnowAboutFailedInitialization();
		//	return;
		//}

		//// Load and initialize the manager
		//if (!InitManager())
		//{
		//	LetUserKnowAboutFailedInitialization();
		//	return;
		//}
	}

	void CLRBridge::InvokeTickEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseTick();
	}
	void CLRBridge::InvokeGameLoadEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseGameLoad();
	}
	void CLRBridge::InvokeGameLoadPriorityEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseGameLoadPriority();

		// Initialize native function hooks
		Native::Hooks::Initialize();
		if (Native::NativeHookFunctions::Initialize())
			Logger::LogDebug("[NativeHook] Done!");
	}
	void CLRBridge::InvokeMountDeviceEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;
		
		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseMountDevice();
	}
	void CLRBridge::InvokeDrawingEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseDrawing();
	}
	void CLRBridge::InvokeProcessCameraEvents()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseProcessCamera();
	}
	void CLRBridge::InvokeProcessAutomobileEvents(uint32_t* vehPtr)
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseProcessAutomobile(UIntPtr(vehPtr));
	}
	void CLRBridge::InvokeProcessPadEvents(uint32_t* padPtr)
	{
		if (CLRBridge::IsBridgeDisabled)
			return;
		if (!ManagerScript::HasInstance())
			return;

		// Disable inputs if ImGui wants text input
		if ((ImGuiIV::DisableMouseInput || ImGuiIV::DisableKeyboardInput) && !ManagerScript::GetInstance()->IsUsingController())
		{
			CPad* pad = reinterpret_cast<CPad*>(padPtr);

			for (int i = 0; i < 187; i++)
			{
				tPadValues* values = &pad->m_aValues[i];
				IVSDKDotNet::Enums::ePadControls control = (IVSDKDotNet::Enums::ePadControls)i;

				if (ImGuiIV::DisableMouseInput)
				{
					switch (control)
					{
						// All of these have to be set to 128, otherwise they can produce some ugly results
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_DOWN:

						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_DOWN:

						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_X:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_Y:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_RIGHT_AXIS_X:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_RIGHT_AXIS_Y:

						case IVSDKDotNet::Enums::ePadControls::INPUT_MOUSE_UD:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOUSE_LR:

						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_KEY_STUNTJUMP:

						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_UD:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_LR:
							values->m_nCurrentValue = 128;
							values->m_nLastValue = 128;
							break;

						// Do nothing with these
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_DOWN:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_DOWN:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_LEFT_2:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_RIGHT_2:
							break;
					}
				}
				if (ImGuiIV::DisableKeyboardInput)
				{
					switch (control)
					{
						// All of these have to be set to 128, otherwise they can produce some ugly results
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_DOWN:

						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_DOWN:

						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_GUN_DOWN:

						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_X:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_Y:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_RIGHT_AXIS_X:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_RIGHT_AXIS_Y:

						case IVSDKDotNet::Enums::ePadControls::INPUT_MOVE_KEY_STUNTJUMP:

						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_UD:
						case IVSDKDotNet::Enums::ePadControls::INPUT_FRONTEND_AXIS_LR:

						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_LEFT_2:
						case IVSDKDotNet::Enums::ePadControls::INPUT_VEH_MOVE_RIGHT_2:
							values->m_nCurrentValue = 128;
							values->m_nLastValue = 128;
							break;

						// Do nothing with these
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_LEFT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_RIGHT:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_UP:
						case IVSDKDotNet::Enums::ePadControls::INPUT_LOOK_DOWN:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOUSE_UD:
						case IVSDKDotNet::Enums::ePadControls::INPUT_MOUSE_LR:
							break;

						// Set all the other stuff to 0
						default:
							values->m_nCurrentValue = 0;
							values->m_nLastValue = 0;
							break;
					}
				}
			}
		}

		// ManagerScript things
		if (ManagerScript::GetInstance()->SwitchImGuiForceCursorProperty)
		{
			ImGuiIV::ForceCursor = !ImGuiIV::ForceCursor;
			ManagerScript::GetInstance()->SwitchImGuiForceCursorProperty = false;
		}

		// Raise event for all subscribers
		ManagerScript::GetInstance()->RaiseProcessPad(UIntPtr(padPtr));
	}
	void CLRBridge::InvokeIngameStartupEvent()
	{
		if (CLRBridge::IsBridgeDisabled)
			return;

		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->RaiseIngameStartup();
	}

	bool CLRBridge::DoEarlyChecks()
	{
		if (!File::Exists(m_sIVSDKDotNetManagerPath))
		{
			Logger::LogError("Could not initialize IV-SDK .NET because the IVSDKDotNet.Manager.dll file was not found inside the IVSDKDotNet directory!", false);
			return false;
		}
		if (!File::Exists(IVSDKDotNetDataPath + "\\public-sans.regular.ttf"))
		{
			Logger::LogError("Could not initialize IV-SDK .NET because the public-sans.regular.ttf file was not found inside the IVSDKDotNet->Data directory!", false);
			return false;
		}
		if (!File::Exists(IVSDKDotNetBinaryPath + "\\Newtonsoft.Json.dll"))
		{
			Logger::LogError("Could not initialize IV-SDK .NET because the Newtonsoft.Json.dll file was not found inside the IVSDKDotNet->bin directory!", false);
			return false;
		}

		return true;
	}
	void CLRBridge::DoAdditionalChecks()
	{
		if (Settings->GetBoolean("Scripts", "LoadScriptHookDotNetScripts", true))
		{
			if (File::Exists("ScriptHookDotNet.dll") || File::Exists("ScriptHookDotNet.asi"))
			{
				String^ msg = String::Format("Incompability detected!{0}{0}"
					+ "Please remove the old ScriptHookDotNet if you want to use the ScriptHookDotNet mod loader from IV-SDK .NET!{0}{0}"
					+ "The files that would need to be removed: ScriptHookDotNet.dll and ScriptHookDotNet.asi{0}{0}"
					+ "- Why am i seeing this message?{0}"
					+ "You are seeing this message because the 'LoadScriptHookDotNetScripts' option in the IV-SDK .NET config file is enabled, which enables the experimental ScriptHookDotNet mod loader of IV-SDK .NET. For more information about this, open the Console and type in 'wiki' to get to the IV-SDK .NET GitHub Wiki page.{0}{0}"
					+ "Would you like to close the game now and remove the old ScriptHookDotNet?", Environment::NewLine);

				switch (MessageBox::Show(msg,
					"IV-SDK .NET",
					MessageBoxButtons::YesNo,
					MessageBoxIcon::Warning,
					MessageBoxDefaultButton::Button1,
					MessageBoxOptions::DefaultDesktopOnly))
				{
					case DialogResult::Yes:
						Environment::Exit(0);
						break;
					case DialogResult::No:
						DisableScriptHookDotNetLoading = true;
						MessageBox::Show("Disabled the IV-SDK .NET ScriptHookDotNet mod loader to avoid conflicts.", "IV-SDK .NET", MessageBoxButtons::OK, MessageBoxIcon::Warning, MessageBoxDefaultButton::Button1, MessageBoxOptions::DefaultDesktopOnly);
						break;
				}
			}
		}
	}
	bool CLRBridge::DoEarlyAdditionalAssemblyLoading()
	{
		try
		{
			// Load Newtonsoft.Json assembly
			Assembly::UnsafeLoadFrom(IVSDKDotNetBinaryPath + "\\Newtonsoft.Json.dll");
			Logger::LogDebug("Early loaded the Newtonsoft.Json.dll assembly.");

			return true;
		}
		catch (Exception^ ex)
		{
			Logger::LogError(String::Format("An exception occured while trying to early load some additional assemblies. Details: {0}", ex->ToString()));

#if _DEBUG
			if (Debugger::IsAttached)
				throw;
#endif // _DEBUG
		}

		return false;
	}
	bool CLRBridge::InitManager()
	{
		try
		{
			// Load the manager assembly
			Assembly^ assembly = Assembly::UnsafeLoadFrom(m_sIVSDKDotNetManagerPath);

			if (!assembly)
			{
				Logger::LogError("Failed to load the manager assembly!");
				return false;
			}

			// Get types from assembly
			array<Type^>^ containedTypes = assembly->GetTypes();
			for (int i = 0; i < containedTypes->Length; i++)
			{
				Type^ containedType = containedTypes[i];

				if (!containedType->IsSubclassOf(ManagerScript::typeid))
					continue;

				// Create new instance of type for assembly
				ManagerScript^ ms = (ManagerScript^)assembly->CreateInstance(containedType->FullName);

				if (!ms)
				{
					Logger::LogError("Failed to create new instance of ManagerScript!");
					LetUserKnowAboutFailedInitialization();
					return false;
				}

				// Set static instance
				ManagerScript::SetInstance(ms);

				// Apply Settings
				ms->ApplySettings(Settings);

				// Create and set dummy script for manager
				ms->SetDummyScript(gcnew Script(true, Guid("00000000-0000-0000-0000-000000000001")));

				// Load plugins
				if (!Settings->GetBoolean("DEBUG", "DisablePluginsLoadOnStartup", false))
					ms->LoadPlugins();

				return true;
			}
		}
		catch (ReflectionTypeLoadException^ ex)
		{
			array<Exception^>^ exs = ex->LoaderExceptions;
			for (int i = 0; i < exs->Length; i++)
			{
				Exception^ e = exs[i];
				Logger::LogError(String::Format("A ReflectionTypeLoadException occured while trying to load the IV-SDK .NET Manager. Details: {0}", e), false);
			}

#if _DEBUG
			if (Debugger::IsAttached)
				throw;
#endif // _DEBUG
		}
		catch (Exception^ ex)
		{
			Logger::LogError(String::Format("An exception occured while trying to load the IV-SDK .NET Manager. Details: {0}", ex));

#if _DEBUG
			if (Debugger::IsAttached)
				throw;
#endif // _DEBUG
		}

		return false;
	}
	void CLRBridge::LoadScripts()
	{
		try
		{
			if (Settings->GetBoolean("DEBUG", "DisableScriptLoadOnStartup", false))
				return;

			ManagerScript::GetInstance()->LoadScripts();
		}
		catch (ReflectionTypeLoadException^ ex)
		{
			array<Exception^>^ exs = ex->LoaderExceptions;
			for (int i = 0; i < exs->Length; i++)
			{
				Exception^ e = exs[i];
				Logger::LogError(String::Format("A ReflectionTypeLoadException occured while trying to automatically load all scripts. Details: {0}", e), false);
			}

#if _DEBUG
			if (Debugger::IsAttached)
				throw;
#endif // _DEBUG
		}
		catch (Exception^ ex)
		{
			Logger::LogError(String::Format("An exception occured while trying to automatically load all scripts. Details: {0}", ex));

#if _DEBUG
			if (Debugger::IsAttached)
				throw;
#endif // _DEBUG
		}
	}
	void CLRBridge::LetUserKnowAboutFailedInitialization()
	{
		PreInitializationShutdown();
		MessageBox::Show("IV-SDK .NET got disabled due to errors while trying to load it. Check the 'IVSDKDotNet.log' file, to find out what went wrong.", "IV-SDK .NET", MessageBoxButtons::OK, MessageBoxIcon::Warning);
	}
	void CLRBridge::PreInitializationShutdown()
	{
		IsBridgeDisabled = true;

		// MinHook
		ShutdownMinHook();

		// Logger
		Logger::ForceCreateLogFile();
		Logger::Shutdown();

		// Other
		if (Settings)
		{
			Settings->Clear();
			Settings = nullptr;
		}
	}
	void CLRBridge::ShutdownMinHook()
	{
		if (m_bHasMinHookInitialized)
		{
			MH_STATUS minHookStatus = MH_DisableHook(MH_ALL_HOOKS);

			if (minHookStatus != MH_STATUS::MH_OK)
				Logger::LogError(String::Format("[MinHook] Failed to disable Hooks! Details: {0}", gcnew String(MH_StatusToString(minHookStatus))), false);

			minHookStatus = MH_Uninitialize();

			if (minHookStatus != MH_STATUS::MH_OK)
				Logger::LogError(String::Format("[MinHook] Failed to uninitialize! Details: {0}", gcnew String(MH_StatusToString(minHookStatus))), false);

			m_bHasMinHookInitialized = false;
		}
	}
	void CLRBridge::Cleanup()
	{
		// Return if already shutting down
		if (IsShuttingDown)
			return;

		IsShuttingDown = true;

		// Log
		Logger::Log("Application is closing, starting cleanup process...");

		// Start cleanup in manager script
		if (ManagerScript::HasInstance())
			ManagerScript::GetInstance()->Cleanup();

		// MinHook
		ShutdownMinHook();

		// ImGui
		ImGuiDraw::UninitializeImGui();

		// Other
		if (Settings)
		{
			Settings->Clear();
			Settings = nullptr;
		}

		// Logger
		Logger::Log("Cleanup process finished!");
		Logger::ForceCreateLogFile();
		Logger::Shutdown();

		CanTerminate = true;
	}

}
