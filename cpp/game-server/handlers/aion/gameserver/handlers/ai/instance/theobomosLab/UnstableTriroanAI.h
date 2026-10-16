#pragma once

#include <string_view>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::theobomosLab {

/**
 * The AI of Unstable Triroan in Theobomos Lab ("triroan"): at 99 % HP he casts 16699 on himself; from 90 % down every phase (90, 80, ... 10,
 * 5) sends element summons walking to him from (601.97, 488.85): fire 280975, water 280976, earth 280977, wind 280978 on routes
 * 3101100002-3101100005, one to four a phase. Attacked at full HP after a phase, or back home, his phases start over.
 * <p>
 * Java: data/handlers/ai/instance/theobomosLab/UnstableTriroanAI.java, @AIName("triroan") (the marker is in the .cpp).
 *
 * @author Ritsu
 */
class UnstableTriroanAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	// Java: = new HpPhases(99, 90, 80, 70, 60, 50, 40, 30, 20, 10, 5)
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(99, {90, 80, 70, 60, 50, 40, 30, 20, 10, 5});

public:
	explicit UnstableTriroanAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnFire();

	void spawnWater();

	void spawnEarth();

	void spawnWind();

	void startWalk(Npc& npc, std::string_view walkId);

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::theobomosLab
