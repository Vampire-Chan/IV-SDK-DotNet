#define VALIDATE_SIZE(struc, size) static_assert(sizeof(struc) == size, "Invalid structure size of " #struc)
#define VALIDATE_OFFSET(struc, member, offset) \
	static_assert(offsetof(struc, member) == offset, "The offset of " #member " in " #struc " is not " #offset "...")

#ifndef INCLUDE_GUARD
#define INCLUDE_GUARD

#pragma comment(lib, "version.lib")

namespace plugin
{
	enum eGameVersion
	{
		VERSION_NONE,
		VERSION_1070,
		VERSION_1080,
	};
	static eGameVersion gameVer = VERSION_NONE;
	void gameStartupEvent();
}

#include "..\Hooking.Patterns.h"

#include "NewAddressSet.h"
#include "CRGBA.h"
#include "rage.h"
#include "CRect.h"
#include "CVector.h"
#include "CVector2D.h"
#include "CQuaternion.h"
#include "CMatrix.h"
#include "CPlayerInfo.h"
#include "CWorld.h"
#include "CPad.h"
#include "CSimpleTransform.h"
#include "CEntity.h"
#include "CDynamicEntity.h"
#include "CBuilding.h"
#include "CInteriorInst.h"
#include "CPhysical.h"
#include "CCustomShaderEffectBase.h"
#include "CObject.h"
#include "CPool.h"
#include "CPools.h"
#include "CTask.h"
#include "CTaskComplexWander.h"
#include "CTaskComplexDie.h"
#include "CTaskComplexNewGetInVehicle.h"
#include "CTaskComplexClimbLadder.h"
#include "CTaskComplexPlayerSettingsTask.h"
#include "CTaskComplexInWater.h"
#include "CTaskComplexMobileMakeCall.h"
#include "CTaskComplexPlayerOnFoot.h"
#include "CTaskComplexNM.h"
#include "CTaskSimpleNMJumpRollFromRoadVehicle.h"
#include "CTaskSimpleSidewaysDive.h"
#include "CTaskSimpleNMHighFall.h"
#include "CPed.h"
#include "CPedFactoryNY.h"
#include "CVehicleFactoryNY.h"
#include "CRadar.h"
#include "CGarage.h"
#include "cHandlingDataMgr.h"
#include "CVehicle.h"
#include "CModelInfo.h"
#include "phBound.h"
#include "phInstGta.h"
#include "CTheScripts.h"
#include "phConstrainedCollider.h"
#include "CTimer.h"
#include "CStreaming.h"
#include "CFileLoader.h"
#include "CdStream.h"
#include "CGame.h"
#include "CCam.h"
#include "CCamera.h"
#include "CExplosion.h"
#include "CHudColours.h"
#include "CVisibilityPlugins.h"
#include "CNetwork.h"
#include "CShadows.h"

#include "CWeaponInfo.h"

#include "CClock.h"
#include "CCutsceneMgr.h"
#include "CRenderPhase.h"
#include "grcTexture.h"
#include "grcTextureFactory.h"
#include "CSprite2d.h"
#include "CDraw.h"
#include "pgDictionary.h"
#include "CTxdStore.h"
#include "CFileMgr.h"
#include "CFont.h"
#include "audRadioAudioEntity.h"
#include "SkyDome.h"
#include "CBaseDC.h"
#include "CDrawRectDC.h"
#include "CDrawCurvedWindowDC.h"
#include "CDrawSpriteDC.h"
#include "CDrawSpriteUVDC.h"
#include "CDrawRadioHudTextDC.h"
#include "CMenuManager.h"
#include "CShaderLib.h"
#include "CTimeCycle.h"
#include "CPickups.h"

#include "fiDevice.h"
#include "fiPackfile.h"
#include "CGameConfigReader.h"

#include "audEngine.h"
#include "CPedType.h"
#include "CQuadTreeNode.h"
#include "CIplStore.h"
#include "CGameLogic.h"
#include "CStats.h"
#include "CGenericGameStorage.h"
#include "CReplay.h"
#include "CAudioZones.h"
#include "CStore.h"
#include "CAnimManager.h"
#include "CPopulation.h"
#include "CWeather.h"
#include "CAchievements.h"
#include "CProjectileInfo.h"
#include "CCheat.h"
#include "CWanted.h"
#include "CText.h"
#include "C_PcSave.h"
#include "CRestart.h"
#include "CTheZones.h"
#include "CGangs.h"
#include "CStuntJumpManager.h"
#include "CAERadioTrackManager.h"
#include "CFrontEnd.h"
#include "CEpisodes.h"

#endif