#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/ai/HpPhases.h"

#include <limits>

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Kaisinel and Marchutan ("empyrean_lord"): 9 s after the spawn the drakan 219532-219539 die; each phase's cast (20932/20936
 * at 8.5 s; 20929 and 20933 on Tiamat); after the first phase's cast they fight Tiamat; the first phase tells its 50 and 15 % hp.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/EmpyreanLordAI.java, @AIName("empyrean_lord") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EmpyreanLordAI : public GeneralNpcAI, public gameserver::ai::HpPhases::PhaseHandler {
private:
	const runtime::Ref<gameserver::ai::HpPhases> hpPhases = gameserver::ai::HpPhases::create(50, {15}); // Java: = new HpPhases(50, 15)
	runtime::Field<int32_t> tiamatId{};

public:
	explicit EmpyreanLordAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

protected:
	void handleAttack(runtime::Ptr<Creature> creature) override;

public:
	void handleHpPhase(int32_t phaseHpPercent) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
