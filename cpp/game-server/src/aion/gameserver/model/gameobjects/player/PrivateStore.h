#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/trade/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`PartSlot<PrivateStore, RetireTo::RECLAIMER>`, replaced on every
 * store opening), bound to the player in the constructor. removeItem replaces the item map, so the map is a `Field<Ref<RcLinkedHashMap>>`
 * returned as `Ptr`.
 *
 * @author Xav, Simple
 */
class PrivateStore : public runtime::OwnedPart {
private:
	runtime::OwnerRef<Player> owner;
	// Java: = new LinkedHashMap<>() (constructor)
	runtime::Field<runtime::Ref<runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>>> items{};
	runtime::Field<std::string> storeMessage{};

public:
	/** This method binds a player to the store and creates a list of items */
	explicit PrivateStore(Player& owner);

	~PrivateStore() override;

	/** This method will return the owner of the store */
	Player& getOwner() const { return owner; }

	/** This method will return the items being sold */
	runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<trade::TradePSItem>>> getSoldItems() const { return items.get(); }

	/** This method will add an item to the list and price */
	void addItemToSell(int32_t itemObjId, trade::TradePSItem& tradeItem);

	/** This method will remove an item from the list */
	void removeItem(int32_t itemObjId);

	/** @return the trade item, null if the store does not sell it */
	runtime::Ptr<trade::TradePSItem> getTradeItemByObjId(int32_t itemObjId);

	void setStoreMessage(std::string_view value) { storeMessage.set(std::string(value)); }

	/** Java returns "" for a null message; the C++ field holds "" for null */
	std::string getStoreMessage() const { return storeMessage.get(); }
};

} // namespace aion::gameserver::model::gameobjects::player
