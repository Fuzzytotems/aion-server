#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

/**
 * The AI of Warden Surama in Esoterrace ("wardensurama"): at 50, 25 and 5 % HP she casts 19332 and the eight geysers rise for 13 seconds;
 * back home, the phases start again.
 * <p>
 * Java: data/handlers/ai/instance/esoterrace/WardenSuramaAI.java, @AIName("wardensurama"). C++: the scheduled despawn is pinned to this AI.
 *
 * @author xTz
 */
class WardenSuramaAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50, {25, 5}); // Java: = new HpPhases(50, 25, 5)

public:
	explicit WardenSuramaAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnGeysers();

	void despawnGeysers();

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
