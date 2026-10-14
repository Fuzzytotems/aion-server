#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

namespace aion::gameserver::handlers::ai::instance::kromedesTrial {

/**
 * The AI of Kromede's Trial's item objects ("krobject"): the use opens the dialog, which gives the object's skill item once, while the
 * player has none.
 * <p>
 * Java: data/handlers/ai/instance/kromedesTrial/KromedesItemNpcsAI.java, @AIName("krobject") (the marker is in the .cpp).
 *
 * @author Gigi, xTz
 */
class KromedesItemNpcsAI : public ActionItemNpcAI {
public:
	explicit KromedesItemNpcsAI(Npc& owner) : ActionItemNpcAI(owner) {}

	bool onDialogSelect(Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) override;

protected:
	void handleUseItemFinish(Player& player) override;
};

} // namespace aion::gameserver::handlers::ai::instance::kromedesTrial
