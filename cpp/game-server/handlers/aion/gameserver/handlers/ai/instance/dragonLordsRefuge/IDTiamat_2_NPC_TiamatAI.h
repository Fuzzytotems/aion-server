#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge {

/**
 * The AI of Tiamat's first form ("IDTiamat_2_NPC_Tiamat"): peaceful; after 20919 her dragon form (the next npc id) comes and she goes.
 * <p>
 * Java: data/handlers/ai/instance/dragonLordsRefuge/IDTiamat_2_NPC_TiamatAI.java, @AIName("IDTiamat_2_NPC_Tiamat") (the marker is in the .cpp).
 *
 * @author Estrayl
 */
class IDTiamat_2_NPC_TiamatAI : public GeneralNpcAI {
public:
	explicit IDTiamat_2_NPC_TiamatAI(Npc& owner) : GeneralNpcAI(owner) {}

protected:
	void handleBeforeSpawned() override;

public:
	void onEndUseSkill(const SkillTemplate* skillTemplate, int32_t skillLevel) override;
};

} // namespace aion::gameserver::handlers::ai::instance::dragonLordsRefuge
