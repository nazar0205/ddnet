
#include "aimbot.h"

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>

#include <algorithm>

namespace OpenKrx
{
	CAimbot::CAimbot() = default;

	CAimbot::~CAimbot() = default;

	void CAimbot::OnReset()
	{
		m_vCachedTargets.clear();
		m_pTargetWorld.reset();

		m_HookTarget = -1;
		m_WeaponTarget = -1;
		m_ClientId = -1;
		m_Slot = -1;

		m_HookAim = vec2(0, 0);
		m_HookAcquired = false;
		m_AutoHookIssued = false;

		m_FireLength = 0;
		m_FireCounter = -1;
	}

	std::optional<vec2> CAimbot::FindDragAim(
		CGameWorld &Source,
		int ClientId,
		CGameClient *pGameClient,
		vec2 PhysicalMouse)
	{
		(void)pGameClient;
		(void)PhysicalMouse;

		auto *pLocal = Source.GetCharacterById(ClientId);
		if(!pLocal)
			return std::nullopt;

		if(m_ClientId != ClientId)
		{
			OnReset();
			m_ClientId = ClientId;
		}

		m_HookTarget = -1;

		const int TuneZone = std::clamp(
			pLocal->GetOverriddenTuneZone(),
			0,
			TuneZone::NUM - 1);

		const float MaxDistance =
			Source.TuningList()[TuneZone].m_HookLength + 30.0f;

		float BestDistance = MaxDistance;

		for(CEntity *pEntity = Source.FindFirst(CGameWorld::ENTTYPE_CHARACTER);
			pEntity;
			pEntity = pEntity->TypeNext())
		{
			auto *pTarget = static_cast<CCharacter *>(pEntity);
			const int TargetId = pTarget->GetCid();

			if(TargetId == ClientId)
				continue;

			if(!pTarget->CanCollide(ClientId))
				continue;

			if(pLocal->Core()->m_HookHitDisabled)
				continue;

			if(pTarget->Core()->m_Solo != pLocal->Core()->m_Solo)
				continue;

			const vec2 Difference = pTarget->m_Pos - pLocal->m_Pos;
			const float Distance = length(Difference);

			if(Distance <= 0.0f || Distance >= BestDistance)
				continue;

			BestDistance = Distance;
			m_HookTarget = TargetId;
		}

		if(m_HookTarget < 0)
			return std::nullopt;

		auto *pTarget = Source.GetCharacterById(m_HookTarget);
		if(!pTarget)
			return std::nullopt;

		return pTarget->m_Pos - pLocal->m_Pos;
	}
}
