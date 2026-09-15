#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemAddType.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/item/fwd.h"

namespace aion::gameserver::services::item {

/**
 * C++: a static-only class (hub-headers.md §11.1). ItemUpdatePredicate is RefCounted (subclasses: DropService, CraftService callbacks); its
 * constructors and changeItem are ported, so DEFAULT_UPDATE_PREDICATE exists from static initialization on.
 *
 * @author KID
 */
class ItemService {
public:
	class ItemUpdatePredicate : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		// fieldmap.toml: ItemPacketService::ItemUpdateType is the generated ItemPacketService_ItemUpdateType (ItemPacketService.h not included)
		const ItemPacketService_ItemUpdateType itemUpdateType;
		// fieldmap.toml: ItemPacketService::ItemAddType is the generated ItemPacketService_ItemAddType (ItemPacketService.h not included)
		const ItemPacketService_ItemAddType itemAddType;
	protected:
		ItemUpdatePredicate(ItemPacketService_ItemAddType itemAddType, ItemPacketService_ItemUpdateType itemUpdateType);
	public:
		static runtime::Ref<ItemService::ItemUpdatePredicate> create(ItemPacketService_ItemAddType value, ItemPacketService_ItemUpdateType itemUpdateTypeValue);
	protected:
		ItemUpdatePredicate();
	public:
		static runtime::Ref<ItemService::ItemUpdatePredicate> create();
		ItemPacketService_ItemUpdateType getUpdateType(model::gameobjects::Item& item, bool isIncrease);
		ItemPacketService_ItemAddType getAddType() const { return this->itemAddType; }
		virtual bool changeItem(model::gameobjects::Item& item);
	protected:
		~ItemUpdatePredicate() override;
	};
	// during static destruction
	static const runtime::Ref<ItemService::ItemUpdatePredicate>& DEFAULT_UPDATE_PREDICATE;
	static int64_t addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, bool allowInventoryOverflow);
	static int64_t addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count);
	static int64_t addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate);
	/** Add new item based on all sourceItem values */
	static int64_t addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem);
	/** Add new item based on all sourceItem values, but with different count */
	static int64_t addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count);
	/** Add new item based on all sourceItem values, but with different count */
	static int64_t addItem(model::gameobjects::player::Player& player, model::gameobjects::Item& sourceItem, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate);
private:
	/** Add new item based on sourceItem values */
	static int64_t addItem(model::gameobjects::player::Player& player, int32_t itemId, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate);
	/** Add non-stackable item to inventory */
	static int64_t addNonStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, runtime::Ptr<model::gameobjects::Item> sourceItem, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate);
public:
	/** Copy some item values like item stones and enchant level, without any fusion item attributes */
	static void copyItemInfo(model::gameobjects::Item& sourceItem, model::gameobjects::Item& newItem);
private:
	/** Add stackable item to inventory */
	static int64_t addStackableItem(model::gameobjects::player::Player& player, const model::templates::item::ItemTemplate* itemTemplate, int64_t count, bool allowInventoryOverflow, ItemService::ItemUpdatePredicate& predicate);
};

} // namespace aion::gameserver::services::item
