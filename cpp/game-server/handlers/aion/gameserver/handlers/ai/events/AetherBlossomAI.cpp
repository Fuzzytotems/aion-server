#include "aion/gameserver/handlers/ai/events/AetherBlossomAI.h"

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/services/item/ItemService.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(AetherBlossomAI, "aether_blossom");

// Java AetherBlossomAI.java:27-33
void AetherBlossomAI::handleUseItemFinish(Player& player) {
	if (hasRewarded.compareAndSet(false, true)) // just in case
		ItemService::addItem(player, 186000406, 1); // [Event] Aether Blossom
	ActionItemNpcAI::handleUseItemFinish(player);
	AIActions::die(*this);
}

} // namespace aion::gameserver::handlers::ai::events
