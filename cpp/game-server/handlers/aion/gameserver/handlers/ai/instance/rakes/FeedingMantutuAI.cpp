#include "aion/gameserver/handlers/ai/instance/rakes/FeedingMantutuAI.h"

#include <any>

#include "aion/gameserver/ai/AIActions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::handlers::ai::instance::rakes {

AION_AI(FeedingMantutuAI, "feeding_mantutu");

// Java FeedingMantutuAI.java:21-27
void FeedingMantutuAI::handleDialogStart(Player& player) {
	runtime::Ptr<WorldMapInstance> instance = getPosition()->getWorldMapInstance();
	if (instance->getNpc(281128) == nullptr && instance->getNpc(281129) == nullptr) {
		ShifterAI::handleDialogStart(player);
	}
}

// Java FeedingMantutuAI.java:29-46. The custom event's argument is the device as a Ref<Npc>, an empty std::any for Java's null.
void FeedingMantutuAI::handleUseItemFinish(Player& player) {
	ShifterAI::handleUseItemFinish(player);
	runtime::Ptr<Npc> boss = getPosition()->getWorldMapInstance()->getNpc(219033);
	if (boss != nullptr && boss->isSpawned() && !boss->isDead()) {
		runtime::Ptr<Npc> npc = nullptr;
		switch (getNpcId()) {
			case 701387: // water supply
				npc = runtime::cast<Npc>(spawn(281129, 712.042f, 490.5559f, 939.7027f, int8_t{0}));
				break;
			case 701386: // feed supply
				npc = runtime::cast<Npc>(spawn(281128, 714.62634f, 504.4552f, 939.60675f, int8_t{0}));
				break;
			default:
				break;
		}
		boss->getAi().onCustomEvent(1, {npc != nullptr ? std::any(runtime::Ref<Npc>(*npc)) : std::any()});
		AIActions::deleteOwner(*this);
	}
}

} // namespace aion::gameserver::handlers::ai::instance::rakes
