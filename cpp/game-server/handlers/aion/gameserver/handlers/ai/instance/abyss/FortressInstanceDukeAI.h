#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/AggressiveNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::abyss {

/**
 * The AI of a fortress instance's duke ("fortress_instance_duke"): skill 18003 brings a summon; the summons (284978-284981) go when the duke
 * dies or despawns.
 * <p>
 * Java: data/handlers/ai/instance/abyss/FortressInstanceDukeAI.java, @AIName("fortress_instance_duke").
 *
 * @author Estrayl
 */
class FortressInstanceDukeAI : public AggressiveNpcAI {
public:
	explicit FortressInstanceDukeAI(Npc& owner) : AggressiveNpcAI(owner) {}

	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;

private:
	void deleteSummons();

protected:
	void handleDied() override;

	void handleDespawned() override;
};

} // namespace aion::gameserver::handlers::ai::instance::abyss
