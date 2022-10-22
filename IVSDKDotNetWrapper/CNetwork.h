#pragma once

class Native_CNetwork
{
public:

	static void* GetClientBroadcastVars()		{ return AddressSetter::GetRef<void*>(0x12C46FC, 0x127111C);	}
	static void* GetClientBroadcastVarSize()	{ return AddressSetter::GetRef<void*>(0x12C46EC, 0x127110C);	}

	//static uint32_t GetIsNetworkSession()		{ return AddressSetter::GetRef<uint32_t>(0xCF806C, 0xD2D77C);	}
	static uint32_t GetNetworkStatus()			{ return AddressSetter::GetRef<uint32_t>(0x13E49B8, 0x1395524); }
	static bool GetIsHostingGame()				{ return AddressSetter::GetRef<bool>(0x14A82FD, 0x14CB4EC);		}

	static bool IsNetworkGameRunning()
	{
		return ((bool(__cdecl*)())(AddressSetter::Get(0x62F90, 0x2C1A10)))();
	}
	static bool IsNetworkSession()
	{
		return ((bool(__cdecl*)())(AddressSetter::Get(0x3B79E0, 0x3D4590)))();
	}
	static void CheckFramerate(bool bUnk, int time)
	{
		((void(__cdecl*)(bool, int))(AddressSetter::Get(0x60310, 0x2BEDD0)))(bUnk, time);
	}
};

namespace IVSDKDotNet {

	public ref class CNetwork
	{
	public:

		// Todo: Add broadcast things

		static void CheckFramerate(bool bUnk, int time);

		static property uint32_t NetworkStatus {
			public: uint32_t get() { return Native_CNetwork::GetNetworkStatus(); }
		}

		static property bool IsHostingGame {
			public: bool get() { return Native_CNetwork::GetIsHostingGame(); }
		}
		static property bool IsNetworkGameRunning {
			public: bool get() { return Native_CNetwork::IsNetworkGameRunning(); }
		}
		static property bool IsNetworkSession {
			public: bool get() { return Native_CNetwork::IsNetworkSession(); }
		}

	};

}