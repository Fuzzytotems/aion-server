#pragma once

#include <vector>

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ActionItemNpcAI.h"

#include "aion/commons/logging/Logger.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai {

/**
 * The AI of a treasure chest ("chest"): its chest template names the keys it takes; opened with them (or without, when a key id is 0), the
 * chest registers its drop for the opener's team members in range (or the opener alone), dies and shows the opener its drop list.
 * <p>
 * Java: data/handlers/ai/ChestAI.java, @AIName("chest"). Java's `Collection<Player> players = new HashSet<>()` keeps the HashSet's iteration
 * order (a Player's hashCode is its object id) through JavaHashMapOrder, since registerDrop hands the members on in that order.
 */
class ChestAI : public ActionItemNpcAI {
public:
	explicit ChestAI(Npc& owner) : ActionItemNpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override;

	void handleUseItemFinish(Player& player) override;

private:
	bool tryOpening(Player& player);

	static int32_t getHighestLevel(const std::vector<runtime::Ptr<Player>>& players);

	/** Java: private ChestTemplate chestTemplate (static data, set by handleDialogStart) */
	runtime::Field<const ChestTemplate*> chestTemplate{nullptr};

	static const commons::logging::Logger log;
};

} // namespace aion::gameserver::handlers::ai
