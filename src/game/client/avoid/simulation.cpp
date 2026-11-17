#include "simulation.h"
#include <algorithm>

#include <engine/engine.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>

#include <game/client/gameclient.h>
#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/entities/pickup.h>
#include <game/collision.h>
#include <game/mapitems.h>

#include <atomic>
#include <chrono>
#include <future>

namespace OpenKrx
{
	namespace
	{
		class CGreedyCandidateJob final : public IJob
		{
			const CGameWorldSimulation &m_World;
			CNetObj_PlayerInput m_Input;
			int m_CheckTicks;
            bool m_AllowDirection;
            bool m_AllowHook;
			std::promise<CBlatantPlan> m_Result;
			std::atomic<bool> m_Claimed{false};

			void Run() override
			{
				RunInline();
			}

		public:
			CGreedyCandidateJob(const CGameWorldSimulation &World, CNetObj_PlayerInput Input,
    int CheckTicks, bool AllowDirection, bool AllowHook) :
    m_World(World),
    m_Input(Input),
    m_CheckTicks(CheckTicks),
    m_AllowDirection(AllowDirection),
    m_AllowHook(AllowHook)
			{
				Abortable(false);
			}
			std::future<CBlatantPlan> Result() { return m_Result.get_future(); }
			void RunInline()
			{
				// A queued job may be claimed by the input thread when the shared
				// engine pool is busy with unrelated work. A late worker then returns
				// without ever dereferencing the (possibly already destroyed) world.
				if(m_Claimed.exchange(true, std::memory_order_acq_rel))
					return;
				auto pPrivateWorld = m_World.CloneForJob();
				m_Result.set_value(CBlatantPlanner::Greedy(
    *pPrivateWorld, m_Input, m_CheckTicks, std::nullopt, true,
    m_AllowDirection, m_AllowHook));
			}
		};
	}

	bool CGameWorldSimulation::CanElideIndependentProbes() const
	{
		// Each Clone owns its events/switchers and rebinds character callbacks.
		// CopyWorldIsolated does not copy PRNG pointers; tele-hook RandomOr0 thus
		// consumes no external stream. Unknown/nonisolated adapters do not opt in.
		// Laser::DoBounce's only collision write requires Res==-1, unreachable
		// through this adapter's IntersectLineTeleWeapon (returns tile IDs or0).
		// This serial probe-elision contract is separate from parallel job safety.
		return !m_World.m_pParent && !m_World.m_pChild && !m_World.m_Core.m_pPrng;
	}

std::vector<CBlatantPlan> CGameWorldSimulation::SearchCandidates(
    const std::vector<CNetObj_PlayerInput> &vInputs,
    int CheckTicks,
    bool AllowDirection,
    bool AllowHook) const
{
    // Search in one concrete world. Keep the existing job-pool optimization,
    // and fall back to deterministic inline simulation when no pool is present.
    const auto SearchWorld = [&](const CGameWorldSimulation &World) {
        if(!World.m_pJobEngine || vInputs.size() < 2)
        {
            std::vector<CBlatantPlan> vInlineResults;
            vInlineResults.reserve(vInputs.size());
            for(const auto &Input : vInputs)
                vInlineResults.push_back(CBlatantPlanner::Greedy(
                    World, Input, CheckTicks, std::nullopt, true, AllowDirection, AllowHook));
            return vInlineResults;
        }

        std::vector<std::future<CBlatantPlan>> vPending;
        std::vector<std::shared_ptr<CGreedyCandidateJob>> vJobs;
        vPending.reserve(vInputs.size());
        vJobs.reserve(vInputs.size());
        for(const auto &Input : vInputs)
        {
            auto pJob = std::make_shared<CGreedyCandidateJob>(
                World, Input, CheckTicks, AllowDirection, AllowHook);
            vPending.push_back(pJob->Result());
            vJobs.push_back(pJob);
            World.m_pJobEngine->AddJob(std::move(pJob));
        }
        for(size_t Index = 0; Index < vPending.size(); ++Index)
        {
            if(vPending[Index].wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                vJobs[Index]->RunInline();
            vPending[Index].wait();
        }
        std::vector<CBlatantPlan> vResults;
        vResults.reserve(vPending.size());
        for(auto &Pending : vPending)
            vResults.push_back(Pending.get());
        return vResults;
    };

    auto vResults = SearchWorld(*this);
    if(!m_PlayerThreatDetected || vInputs.empty())
        return vResults;

    // For a detected player threat, search the same first actions in two
    // opponent continuations: latest inputs and neutral movement/jump with
    // hooks preserved. Rank each candidate by its worse predicted lifespan.
    auto pNeutral = std::unique_ptr<CGameWorldSimulation>(
        static_cast<CGameWorldSimulation *>(Clone().release()));
    if(!pNeutral || !pNeutral->Valid())
        return vResults;

    for(int Id = 0; Id < MAX_CLIENTS; ++Id)
    {
        CCharacter *pCharacter = pNeutral->m_World.GetCharacterById(Id);
        if(!pCharacter || Id == pNeutral->m_LocalClientId || pCharacter->m_IsLocal)
            continue;
        CNetObj_PlayerInput NeutralInput = pCharacter->GetCore().m_Input;
        NeutralInput.m_Direction = 0;
        NeutralInput.m_Jump = 0;
        pCharacter->SetInput(&NeutralInput);
    }

    const auto vNeutralResults = SearchWorld(*pNeutral);
    const size_t Count = std::min(vResults.size(), vNeutralResults.size());
    for(size_t Index = 0; Index < Count; ++Index)
    {
        // Each search starts with the same candidate action. The stored result
        // comes from the worse continuation, so the planner maximizes worst-case
        // survival instead of trusting only one stale opponent input.
        if(vNeutralResults[Index].m_Life < vResults[Index].m_Life)
            vResults[Index] = vNeutralResults[Index];
    }
    return vResults;
}

	
std::unique_ptr<IBlatantSimulation> CGameWorldSimulation::Clone() const
{
	auto pResult = std::make_unique<CGameWorldSimulation>();
	pResult->CopyFrom(m_World, m_LocalClientId);
	pResult->m_pGameClient = m_pGameClient;
	pResult->m_pJobEngine = m_pJobEngine;
	pResult->m_LimitAutomaticFire = m_LimitAutomaticFire;
	pResult->m_AllowImmediateShot = m_AllowImmediateShot;
	pResult->m_AutomaticFireStep = m_AutomaticFireStep;
	return pResult;
}

std::unique_ptr<CGameWorldSimulation> CGameWorldSimulation::CloneForJob() const
{
	return std::unique_ptr<CGameWorldSimulation>(
		static_cast<CGameWorldSimulation *>(Clone().release()));
}


	void CGameWorldSimulation::CopyFrom(const CGameWorld &SourceWorld, int LocalClientId)
{
	m_pJobCollision = std::make_shared<CCollision>();
	m_pJobCollision->CopyFrom(*SourceWorld.Collision());

	m_pJobTuning = std::make_shared<std::array<CTuningParams, 256>>();
	std::copy_n(SourceWorld.TuningList(), 256, m_pJobTuning->begin());

	m_World.CopyWorldIsolated(&SourceWorld, m_pJobCollision.get(), m_pJobTuning->data());

	m_LocalClientId = LocalClientId;
	m_pLocal = LocalClientId >= 0 ? m_World.GetCharacterById(LocalClientId) : nullptr;
	m_pGameClient = nullptr;
	m_pJobEngine = nullptr;
	m_LimitAutomaticFire = m_AllowImmediateShot = false;
	m_AutomaticFireStep = 0;
	m_PlayerThreatDetected = false;
}

	void CGameWorldSimulation::PreparePlayers(bool PredictPlayers, CGameClient *pGameClient)
	{

		m_pGameClient = pGameClient;
		m_pJobEngine = pGameClient ? pGameClient->Engine() : nullptr;
		
		if(!m_pLocal)
			return;
		// 0x1402cd0c0: discard inactive/other-team/distant tees, then remove all
		// remaining non-local tees when krx_avoidtileplayerprediction is disabled.
		const float Radius = HookLength() + 56.0f;
		for(int Id = 0; Id < MAX_CLIENTS; ++Id)
		{
			auto *pCharacter = m_World.GetCharacterById(Id);
			if(!pCharacter || Id == m_LocalClientId || pCharacter->m_IsLocal)
				continue;
	if(!PredictPlayers || distance(m_pLocal->m_Pos, pCharacter->m_Pos) > Radius ||
    (pGameClient && !pGameClient->m_Snap.m_aCharacters[Id].m_Active && pCharacter->m_SnapTicks > 10))
				delete pCharacter;
		}
	}

	void CGameWorldSimulation::Tick(const CNetObj_PlayerInput &Input)
	{
		if(!m_pLocal)
			return;
		CNetObj_PlayerInput Copy = Input;
		if(m_LimitAutomaticFire && (!m_AllowImmediateShot || m_AutomaticFireStep > 0))
		{
			// Use the world's current counter, not the candidate's possibly stale
			// NSIF counter: jumping between even counters can still create presses.
		Copy.m_Fire = m_pLocal->LatestInputFire();
			if(Copy.m_Fire & 1)
				Copy.m_Fire = (Copy.m_Fire + 1) & INPUT_STATE_MASK;
		}
				++m_AutomaticFireStep;
		m_pLocal->OnDirectInput(&Copy);

		if(m_pGameClient)
			m_pGameClient->ApplyPreInputs(m_World.m_GameTick + 1, true, m_World);

		m_World.m_GameTick++;
		m_pLocal->OnPredictedInput(&Copy);

		if(m_pGameClient)
			m_pGameClient->ApplyPreInputs(m_World.m_GameTick, false, m_World);

		m_World.Tick();
		m_pLocal = m_World.GetCharacterById(m_LocalClientId);
	}

	bool CGameWorldSimulation::Frozen() const
	{
		// 2cc900/cd750 read character+318 (tile contact) and +319 (deep),
		// not +31a (persistent live freeze).
		if(!m_pLocal)
			return true;
		return m_pLocal->m_FreezeTime > 0 || m_pLocal->Core()->m_DeepFrozen || m_pLocal->Core()->m_IsInFreeze;
	}
bool CGameWorldSimulation::HookAttached() const
{
    return m_pLocal && m_pLocal->Core()->m_HookState == HOOK_GRABBED;
}
int CGameWorldSimulation::HookState() const
{
    return m_pLocal ? m_pLocal->Core()->m_HookState : HOOK_RETRACTED;
}
bool CGameWorldSimulation::PlayerHookingLocal() const
{
    if(!m_pLocal)
        return false;

    for(int Id : m_pLocal->Core()->m_AttachedPlayers)
    {
        const CCharacter *pCharacter = m_World.GetCharacterById(Id);
        if(!pCharacter || Id == m_LocalClientId)
            continue;

        if(pCharacter->Core()->HookedPlayer() == m_LocalClientId)
            return true;
    }

    return false;
}
vec2 CGameWorldSimulation::HookPullDirection() const
{
    if(!m_pLocal)
        return vec2(0.0f, 0.0f);

    vec2 PullDirection(0.0f, 0.0f);

    for(int Id : m_pLocal->Core()->m_AttachedPlayers)
    {
        const CCharacter *pCharacter = m_World.GetCharacterById(Id);
        if(!pCharacter || Id == m_LocalClientId)
            continue;

        if(pCharacter->Core()->HookedPlayer() != m_LocalClientId)
            continue;

        PullDirection += normalize(pCharacter->m_Pos - m_pLocal->m_Pos);
    }

    if(length(PullDirection) <= 0.001f)
        return vec2(0.0f, 0.0f);

    return normalize(PullDirection);
}

bool CGameWorldSimulation::PredictPlayerThreat(const CNetObj_PlayerInput &Input, int CheckTicks, int Sensitivity, vec2 &Direction, int &TimeToFreeze) const
{
    Direction = vec2(0.0f, 0.0f);
    TimeToFreeze = CBlatantPlan::SAFE;
    m_PlayerThreatDetected = false;
    if(!m_pLocal || Frozen() || CheckTicks < 1)
        return false;

    // Only pay for the counterfactual when another tee can plausibly interact
    // with us: via an incoming hook or by entering the predicted hook/collision
    // neighborhood. A hook merely being attached is not itself a threat.
    bool HasNearbyPlayer = PlayerHookingLocal();
    const float CollisionRadius = CCharacterCore::PhysicalSize() * 2.0f + 8.0f;
    const float InfluenceRadius = HookLength() + 64.0f;
    if(!HasNearbyPlayer)
    {
        for(int Id = 0; Id < MAX_CLIENTS; ++Id)
        {
            const CCharacter *pCharacter = m_World.GetCharacterById(Id);
            if(!pCharacter || Id == m_LocalClientId || pCharacter->m_IsLocal)
                continue;
            const float Distance = distance(m_pLocal->m_Pos, pCharacter->m_Pos);
            const bool CanCollideSoon = Distance <= CollisionRadius;
            const bool HookFlyingAtUs = Distance <= InfluenceRadius &&
                pCharacter->Core()->m_HookState == HOOK_FLYING &&
                dot(normalize(m_pLocal->m_Pos - pCharacter->m_Pos), normalize(pCharacter->Core()->m_HookDir)) > 0.65f;
            if(CanCollideSoon || HookFlyingAtUs)
            {
                HasNearbyPlayer = true;
                break;
            }
        }
    }
    if(!HasNearbyPlayer)
        return false;

    auto CloneWorld = [&]() {
        return std::unique_ptr<CGameWorldSimulation>(
            static_cast<CGameWorldSimulation *>(Clone().release()));
    };
    auto pCurrent = CloneWorld();
    auto pNeutral = CloneWorld();
    auto pWithoutPlayers = CloneWorld();
    if(!pCurrent || !pNeutral || !pWithoutPlayers ||
        !pCurrent->Valid() || !pNeutral->Valid() || !pWithoutPlayers->Valid())
        return false;

    // Scenario A keeps each opponent's latest predicted input.
    // Scenario B assumes opponents release movement/jump but keep any active
    // hook. These two plausible continuations avoid relying on one stale input.
    for(int Id = 0; Id < MAX_CLIENTS; ++Id)
    {
        CCharacter *pCharacter = pNeutral->m_World.GetCharacterById(Id);
        if(!pCharacter || Id == pNeutral->m_LocalClientId || pCharacter->m_IsLocal)
            continue;
        CNetObj_PlayerInput NeutralInput = pCharacter->GetCore().m_Input;
        NeutralInput.m_Direction = 0;
        NeutralInput.m_Jump = 0;
        pCharacter->SetInput(&NeutralInput);
    }

    // Counterfactual: keep the local tee and map physics identical, but remove
    // every other tee. The difference includes incoming hook forces and player
    // collision/push impulses, while excluding freezes caused by the map alone.
    for(int Id = 0; Id < MAX_CLIENTS; ++Id)
    {
        CCharacter *pCharacter = pWithoutPlayers->m_World.GetCharacterById(Id);
        if(!pCharacter || Id == pWithoutPlayers->m_LocalClientId || pCharacter->m_IsLocal)
            continue;
        delete pCharacter;
    }

    int CurrentLife = CBlatantPlan::SAFE;
    int NeutralLife = CBlatantPlan::SAFE;
    int WithoutPlayersLife = CBlatantPlan::SAFE;
    vec2 CurrentDisplacement(0.0f, 0.0f);
    vec2 NeutralDisplacement(0.0f, 0.0f);
    float CurrentDisplacementLength = 0.0f;
    float NeutralDisplacementLength = 0.0f;

    for(int Tick = 0; Tick <= CheckTicks; ++Tick)
    {
        if(CurrentLife == CBlatantPlan::SAFE)
        {
            pCurrent->Tick(Input);
            if(pCurrent->Frozen())
                CurrentLife = Tick;
        }
        if(NeutralLife == CBlatantPlan::SAFE)
        {
            pNeutral->Tick(Input);
            if(pNeutral->Frozen())
                NeutralLife = Tick;
        }
        if(WithoutPlayersLife == CBlatantPlan::SAFE)
        {
            pWithoutPlayers->Tick(Input);
            if(pWithoutPlayers->Frozen())
                WithoutPlayersLife = Tick;
        }

        if((CurrentLife == CBlatantPlan::SAFE || CurrentLife == Tick) &&
            pCurrent->Valid() && pWithoutPlayers->Valid())
        {
            const vec2 Displacement = pCurrent->Position() - pWithoutPlayers->Position();
            const float DisplacementLength = length(Displacement);
            if(DisplacementLength > CurrentDisplacementLength)
            {
                CurrentDisplacement = Displacement;
                CurrentDisplacementLength = DisplacementLength;
            }
        }
        if((NeutralLife == CBlatantPlan::SAFE || NeutralLife == Tick) &&
            pNeutral->Valid() && pWithoutPlayers->Valid())
        {
            const vec2 Displacement = pNeutral->Position() - pWithoutPlayers->Position();
            const float DisplacementLength = length(Displacement);
            if(DisplacementLength > NeutralDisplacementLength)
            {
                NeutralDisplacement = Displacement;
                NeutralDisplacementLength = DisplacementLength;
            }
        }

        if(CurrentLife != CBlatantPlan::SAFE && NeutralLife != CBlatantPlan::SAFE &&
            WithoutPlayersLife != CBlatantPlan::SAFE)
            break;
    }

    // Evaluate the more dangerous of the two opponent continuations.
    const bool NeutralIsWorse = NeutralLife < CurrentLife ||
        (NeutralLife == CurrentLife && NeutralDisplacementLength > CurrentDisplacementLength);
    const int WithPlayersLife = NeutralIsWorse ? NeutralLife : CurrentLife;
    const vec2 PlayerDisplacement = NeutralIsWorse ? NeutralDisplacement : CurrentDisplacement;
    const float PlayerDisplacementLength = NeutralIsWorse ? NeutralDisplacementLength : CurrentDisplacementLength;

    // Trigger only when other players make an otherwise safer trajectory freeze
    // sooner. A harmless hook, or an unavoidable map-only freeze, is not a
    // Player Threat and must not take control away from the user.
    const int RequiredLead = 1 + (10 - std::clamp(Sensitivity, 1, 10)) / 3;
    if(WithPlayersLife == CBlatantPlan::SAFE ||
        (WithoutPlayersLife != CBlatantPlan::SAFE && WithoutPlayersLife - WithPlayersLife < RequiredLead))
        return false;

    TimeToFreeze = WithPlayersLife;
    if(PlayerDisplacementLength > 0.01f)
        Direction = normalize(PlayerDisplacement);
    else
        Direction = HookPullDirection();
    m_PlayerThreatDetected = true;
    return true;
}

	bool CGameWorldSimulation::OnUnfreezeTile() const
	{
		if(!m_pLocal)
			return false;
		// KRX's custom 0x14023cd10 returns the raw game-layer tile. DDNet's
		// GetCollisionAt filters out tile 11 entirely, so it cannot be used here.
		const vec2 Pos(static_cast<int>(m_pLocal->m_Pos.x), static_cast<int>(m_pLocal->m_Pos.y));
		return m_World.Collision()->GetTileIndex(m_World.Collision()->GetPureMapIndex(Pos)) == TILE_UNFREEZE;
	}

	vec2 CGameWorldSimulation::Position() const
	{
		return m_pLocal ? m_pLocal->m_Pos : vec2(0.0f, 0.0f);
	}

	float CGameWorldSimulation::HookLength() const
	{
		// 2cd0c0/2b3980/4460 read client+55a0b4 directly: HookLength at
		// offset 8*sizeof(int) in the first tuning slot (base+55a094).
		// This is independent of the local character's tune zone and dummy slot.
		if(m_pGameClient)
			return m_pGameClient->m_aTuning[0].m_HookLength;
		return m_pLocal ? m_pLocal->GlobalTuning()->m_HookLength : 380.0f;
	}

	std::optional<vec2> CGameWorldSimulation::HookPoint(vec2 Aim) const
	{
		if(!m_pLocal)
			return std::nullopt;
		// 0x1402b3980 first quantizes the aim as an input, then probes the full
		// hook radius irrespective of the mouse's distance from the tee.
		const vec2 QuantizedAim(static_cast<int>(Aim.x), static_cast<int>(Aim.y));
		if(QuantizedAim == vec2(0.0f, 0.0f))
			return std::nullopt;
		const vec2 To = m_pLocal->m_Pos + normalize(QuantizedAim) * (HookLength() - 28.0f);
		vec2 HitPos = To;
		// KRX tests against interpolated live character positions before tracing
		// the map. Keep that test independent of the speculative movement world.
		if(m_pGameClient && m_pGameClient->IntersectCharacter(m_pLocal->m_Pos, To, HitPos, m_LocalClientId) != -1)
			return std::nullopt;
		int TeleNumber = 0;
		const int Hit = m_World.Collision()->IntersectLineTeleHook(m_pLocal->m_Pos, To, &HitPos, nullptr, &TeleNumber);
		if(Hit == TILE_SOLID && HitPos != vec2(0, 0))
			return HitPos;
		return std::nullopt;
	}

	std::unique_ptr<IBlatantSimulation> CreateSimulation(CGameClient *pGameClient, int ClientId, bool PredictPlayers, bool PendingAutomaticFire, bool AllowImmediateShot)
	{
		auto pResult = std::make_unique<CGameWorldSimulation>();
		// 0x1402cd0c0 always starts from the predicted world (client+0x5b3f38).
		// Player prediction only decides whether the snapshot contains other tees.
		// A missing predicted local tee makes cd0c0 fail. Falling back to the
		// authoritative world would silently plan from a different point in time.
		pResult->CopyFrom(pGameClient->m_PredictedWorld, ClientId);
		pResult->PreparePlayers(PredictPlayers, pGameClient);
		if(PendingAutomaticFire)
			pResult->LimitPendingAutomaticFire(AllowImmediateShot);
		return pResult;
	}
}

std::optional<vec2> OpenKrx::CGameWorldSimulation::DragAim()
{
	if(!m_pGameClient || !m_pLocal)
		return std::nullopt;

	const vec2 PhysicalMouse(
		static_cast<float>(m_pLocal->Core()->m_Input.m_TargetX),
		static_cast<float>(m_pLocal->Core()->m_Input.m_TargetY));

	return m_pGameClient->m_Aimbot.FindDragAim(
		m_World,
		m_LocalClientId,
		m_pGameClient,
		PhysicalMouse);
}
