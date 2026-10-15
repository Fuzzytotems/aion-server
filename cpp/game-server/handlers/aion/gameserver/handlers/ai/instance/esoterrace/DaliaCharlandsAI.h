#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

#include <string_view>

#include "aion/gameserver/ai/HpPhases.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::handlers::ai::instance::esoterrace {

/**
 * The AI of Dalia Charlands in Esoterrace ("dalia_charlands"): at 75, 50 and 25 % HP three greenfinger helpers walk out of their routes;
 * above 80 % (healed) and back home, the phases start again.
 * <p>
 * Java: data/handlers/ai/instance/esoterrace/DaliaCharlandsAI.java, @AIName("dalia_charlands").
 *
 * @author xTz
 */
class DaliaCharlandsAI : public AggressiveNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(75, {50, 25}); // Java: = new HpPhases(75, 50, 25)

public:
	explicit DaliaCharlandsAI(Npc& owner) : AggressiveNpcAI(owner) {}

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;

private:
	void spawnHelpers();

	void startWalk(Npc& npc, std::string_view walkId);

protected:
	void handleBackHome() override;
};

} // namespace aion::gameserver::handlers::ai::instance::esoterrace
