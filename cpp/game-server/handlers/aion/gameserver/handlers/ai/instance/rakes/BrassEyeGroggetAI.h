#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/SummonerAI.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

/**
 * The AI of Brass-Eye Grogget on the Steel Rake ("brasseyegrogget"): each individual summons percentage picks a wave by the percentage (61-80:
 * 1, 31-60: 2, up to 30: 3, else 0); 9 s later the wave's first helpers come, 35 s after them the second ones, and 21 s after those his 18191
 * is taken off.
 * <p>
 * Java: data/handlers/ai/instance/rakes/BrassEyeGroggetAI.java, @AIName("brasseyegrogget") (the marker is in the .cpp).
 * <p>
 * C++: Java's private spawn(int) overloads AbstractAI's spawn methods; the using-declaration keeps those visible here.
 *
 * @author xTz
 */
class BrassEyeGroggetAI : public SummonerAI {
public:
	explicit BrassEyeGroggetAI(Npc& owner) : SummonerAI(owner) {}

protected:
	using SummonerAI::spawn;

	void handleSpawned() override;

	void handleIndividualSpawnedSummons(const model::templates::ai::Percentage& percent) override;

private:
	void spawn(int32_t percent);

	void spawnHelpers1(int32_t nrSpawn);

	void spawnHelpers2(int32_t nrSpawn);
};

} // namespace aion::gameserver::handlers::ai::instance::rakes
