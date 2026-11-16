
#ifndef GAME_CLIENT_KRX_PARALLEL_PREDICTION_H
#define GAME_CLIENT_KRX_PARALLEL_PREDICTION_H

#include <game/client/prediction/gameworld.h>
#include <game/collision.h>

#include <algorithm>
#include <array>
#include <functional>

namespace OpenKrx
{
	inline void ParallelPrediction(
		const CGameWorld &Source,
		IEngine *pEngine,
		int Count,
		std::function<void(const CGameWorld &, int)> Evaluate)
	{
		for(int I = 0; I < Count; ++I)
		{
			CCollision Collision;
			Collision.CopyFrom(*Source.Collision());

			std::array<CTuningParams, 256> Tuning;
			std::copy_n(Source.TuningList(), Tuning.size(), Tuning.begin());

			CGameWorld World;
			World.CopyWorldIsolated(&Source, &Collision, Tuning.data());

			Evaluate(World, I);
		}
	}
}

#endif
