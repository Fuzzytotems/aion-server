#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/instance/eternalBastion/EternalBastionAssaulterNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::eternalBastion {

/**
 * The AI of the siege ram ("eternal_bastion_siege_ram"): an assaulter that hates the npcs it meets and rams its target over and over (20778).
 * <p>
 * Java: data/handlers/ai/instance/eternalBastion/EternalBastionSiegeRamAI.java, @AIName("eternal_bastion_siege_ram") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class EternalBastionSiegeRamAI : public EternalBastionAssaulterNpcAI {
public:
	explicit EternalBastionSiegeRamAI(Npc& owner) : EternalBastionAssaulterNpcAI(owner) {}

protected:
	void handleCreatureAggro(Creature& creature) override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::eternalBastion
