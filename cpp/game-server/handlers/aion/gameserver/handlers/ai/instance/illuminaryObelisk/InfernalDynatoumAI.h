#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/illuminaryObelisk/DynatoumAI.h"

namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk {

/**
 * Infernal Dynatoum ("infernal_dynatoum"): Dynatoum with the hard mode's messages and timings.
 * <p>
 * Java: data/handlers/ai/instance/illuminaryObelisk/InfernalDynatoumAI.java, @AIName("infernal_dynatoum") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class InfernalDynatoumAI : public DynatoumAI {
public:
	explicit InfernalDynatoumAI(Npc& owner) : DynatoumAI(owner) {}

protected:
	void scheduleDespawn(int32_t delayInSec) override;

	void removeBossEntries() override;

public:
	void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::illuminaryObelisk
