#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/drop/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/drop/fwd.h"

namespace aion::gameserver::services::drop {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * TempTradeDropPredicate is declared here and defined in the .cpp (only the bodies use it).
 *
 * @author ATracer, xTz
 */
class DropService : public runtime::Immortal {
private:
	class TempTradeDropPredicate;
	DropService();
	~DropService();
public:
	static DropService& getInstance(); // Java singleton
	void scheduleFreeForAll(int32_t npcUniqueId);
	/** After NPC despawns */
	void unregisterDrop(model::gameobjects::Npc& npc);
	/** When player clicks on dead NPC to request drop list */
	void requestDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId);
	/** This method will change looted corpse to not in use */
	void closeDropList(model::gameobjects::player::Player& player, int32_t npcObjectId);
	bool canDistribute(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem);
	bool canAutoLoot(model::gameobjects::player::Player& player, model::drop::DropItem& requestedItem);
	void requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex);
	void requestDropItem(model::gameobjects::player::Player& player, int32_t npcObjectId, int32_t itemIndex, bool autoLoot);
private:
	static void distributeEqually(model::drop::DropItem& item, const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players);
	void resendDropList(runtime::Ptr<model::gameobjects::player::Player> player, int32_t npcObjectId, model::gameobjects::DropNpc& dropNpc, const std::unordered_set<runtime::Ptr<model::drop::DropItem>>& dropItems);
	void winningRollActions(model::gameobjects::player::Player& player, int32_t itemId, int32_t npcObjectId);
	void winningBidActions(model::gameobjects::player::Player& player, int32_t npcObjectId, int64_t highestValue);
	void winningNormalActions(runtime::Ptr<model::gameobjects::player::Player> player, runtime::Ptr<model::gameobjects::DropNpc> dropNpc, model::drop::DropItem& requestedItem);
public:
	void see(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc);
private:
	void announceDrop(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* template_);
};

} // namespace aion::gameserver::services::drop
