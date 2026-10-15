#pragma once

#include "aion/gameserver/handlers/ai/AiPrelude.h"
#include "aion/gameserver/handlers/ai/ChestAI.h"
#include "aion/gameserver/runtime/fields/Field.h"

namespace aion::gameserver::handlers::ai::instance::kamarBf {

/**
 * The AI of the Reian supply items in Kamar Battlefield ("reiansupplyitems"): a chest with a flag npc, deleted with it.
 * <p>
 * Java: data/handlers/ai/instance/kamarBf/ReianSupplyItemsAI.java, @AIName("reiansupplyitems") (the marker is in the .cpp).
 *
 * @author Cheatkiller
 */
class ReianSupplyItemsAI : public ChestAI {
private:
	runtime::Field<runtime::Ref<Npc>> flag{}; // Java: Npc flag (null until spawned)

public:
	explicit ReianSupplyItemsAI(Npc& owner) : ChestAI(owner) {}

protected:
	void handleSpawned() override;

	void handleDespawned() override;

	void handleDied() override;
};

} // namespace aion::gameserver::handlers::ai::instance::kamarBf
