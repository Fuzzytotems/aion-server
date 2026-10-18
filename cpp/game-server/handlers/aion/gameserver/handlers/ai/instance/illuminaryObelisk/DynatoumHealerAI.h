#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * Dynatoum's healers ("dynatoum_healer"): no thinking; they target Dynatoum and heal it every 10 s.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/DynatoumHealerAI.java, @AIName("dynatoum_healer") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class DynatoumHealerAI : public GeneralNpcAI {
public:
	explicit DynatoumHealerAI(Npc& owner) : GeneralNpcAI(owner) {}

	bool canThink() override;

protected:
	void handleSpawned() override;

private:
	void startHealing();

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
