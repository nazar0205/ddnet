
#ifndef GAME_CLIENT_KRX_PARALLEL_PREDICTION_H
#define GAME_CLIENT_KRX_PARALLEL_PREDICTION_H

#include <engine/shared/protocol.h>

#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

#include <algorithm>
#include <array>
#include <functional>

class IEngine;

namespace OpenKrx
{
	inline void ParallelPrediction(
		const CGameWorld &Source,
		IEngine *pEngine,
		int Count,
		std::function<void(const CGameWorld &, int)> Evaluate)
	{
		(void)pEngine;

		if(Count <= 0)
			return;

		if(!Source.Collision() || !Source.TuningList())
			return;

		CCollision Collision;
		Collision.CopyFrom(*Source.Collision());

		std::array<CTuningParams, TuneZone::NUM> Tuning{};
		std::copy_n(Source.TuningList(), Tuning.size(), Tuning.begin());

		CGameWorld World;
		World.CopyWorldIsolated(&Source, &Collision, Tuning.data());

		for(int I = 0; I < Count; ++I)
			Evaluate(World, I);
	}
}

#endif
