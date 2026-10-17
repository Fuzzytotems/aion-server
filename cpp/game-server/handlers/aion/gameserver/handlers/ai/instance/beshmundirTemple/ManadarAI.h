#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

namespace aion::gameserver::handlers::ai::instance::beshmundirTemple {

/**
 * The AI of Manadar ("manadar"): from 90 % hp every 6 s five traps (281545 or 281756) around it; back home it starts over.
 * <p>
 * Java: data/handlers/ai/instance/beshmundirTemple/ManadarAI.java, @AIName("manadar") (the marker is in the .cpp).
 *
 * @author unknown
 */
class ManadarAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(90); // Java: = new HpPhases(90)

public:
	explicit ManadarAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

protected:
	void handleBackHome() override;

private:
	void trySpawnTraps();
};

} // namespace aion::gameserver::handlers::ai::instance::beshmundirTemple
