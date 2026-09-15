#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob.h"

#include <memory>
#include <vector>

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/ChargeInfo.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/iteminfo/AccessoryInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ArmorInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ArrowInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/BonusInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/CompositeItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ConditioningInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/EquippedSlotBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/GeneralInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/PlumeInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/PolishInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/PremiumOptionInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/ShieldInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/StigmaInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/StigmaShardInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/WeaponInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/WingInfoBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/WrapInfoBlobEntry.h"
#include "aion/gameserver/network/detail/ItemData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::network::aion::iteminfo {

using model::gameobjects::Item;
using model::gameobjects::player::Player;
using model::templates::item::enums::ItemGroup;

runtime::Ref<ItemBlobEntry> newBlobEntry(ItemInfoBlob_ItemBlobType type) {
	switch (type) {
		case ItemInfoBlob_ItemBlobType::GENERAL_INFO:
			return GeneralInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_WEAPON:
			return WeaponInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_ARMOR:
			return ArmorInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_SHIELD:
			return ShieldInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_ACCESSORY:
			return AccessoryInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_ARROW:
			return ArrowInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::EQUIPPED_SLOT:
			return EquippedSlotBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::STIGMA_INFO: // Removed from 3.5
			return StigmaInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::STIGMA_SHARD: // Removed from 4.5, added back in 4.7
			return StigmaShardInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::PREMIUM_OPTION:
			return PremiumOptionInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::POLISH_INFO:
			return PolishInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::WRAP_INFO:
			return WrapInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::PLUME_INFO:
			return PlumeInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::STAT_BONUSES:
			return BonusInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::ENCHANT_INFO:
			return EnchantInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::SLOTS_WING:
			return WingInfoBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::COMPOSITE_ITEM:
			return CompositeItemBlobEntry::create();
		case ItemInfoBlob_ItemBlobType::CONDITIONING_INFO:
			return ConditioningInfoBlobEntry::create();
	}
	throw runtime::IllegalArgumentException("Unknown ItemBlobType");
}

ItemInfoBlob::ItemInfoBlob(runtime::Ptr<Player> playerValue, Item& itemValue) : player(playerValue), item(itemValue) {
}

ItemInfoBlob::~ItemInfoBlob() = default;

runtime::Ref<ItemInfoBlob> ItemInfoBlob::create(runtime::Ptr<Player> playerValue, Item& itemValue) {
	return runtime::makeRef<ItemInfoBlob>(playerValue, itemValue);
}

void ItemInfoBlob::writeMe(commons::utils::ByteBuffer& buf) {
	writeH(buf, size());
	for (runtime::Ptr<ItemBlobEntry> ent : itemBlobEntries)
		ent->writeMe(buf);
}

void ItemInfoBlob::addBlobEntry(ItemBlobType type) {
	runtime::Ref<ItemBlobEntry> ent = iteminfo::newBlobEntry(type);
	ent->setOwner(player, *item, nullptr);
	itemBlobEntries.add(ent);
}

void ItemInfoBlob::addBonusBlobEntry(model::stats::calc::functions::IStatFunction& modifier) {
	runtime::Ref<ItemBlobEntry> ent = iteminfo::newBlobEntry(ItemBlobType::STAT_BONUSES);
	ent->setOwner(player, *item, runtime::Ptr<model::stats::calc::functions::IStatFunction>(modifier));
	itemBlobEntries.add(ent);
}

runtime::Ref<ItemBlobEntry> ItemInfoBlob::newBlobEntry(ItemBlobType type, runtime::Ptr<Player> playerValue, Item& itemValue) {
	if (type == ItemBlobType::STAT_BONUSES)
		throw runtime::UnsupportedOperationException(""); // Java: new UnsupportedOperationException()
	runtime::Ref<ItemBlobEntry> ent = iteminfo::newBlobEntry(type);
	ent->setOwner(playerValue, itemValue, nullptr);
	return ent;
}

runtime::Ref<ItemInfoBlob> ItemInfoBlob::getFullBlob(runtime::Ptr<Player> playerValue, Item& itemValue) {
	runtime::Ref<ItemInfoBlob> blob = create(playerValue, itemValue);
	const model::templates::item::ItemTemplate* itemTemplate = itemValue.getItemTemplate();
	if (itemValue.hasFusionedItem() || itemTemplate->isTwoHandWeapon())
		blob->addBlobEntry(ItemBlobType::COMPOSITE_ITEM);
	if (itemTemplate->getItemSlot() != 0) { // Java: itemTemplate.getItemGroup().getValidEquipmentSlots() (ItemTemplate.getItemSlot returns it)
		// EQUIPPED SLOT
		blob->addBlobEntry(ItemBlobType::EQUIPPED_SLOT);
		if (itemTemplate->getItemGroup() == ItemGroup::WING) {
			blob->addBlobEntry(ItemBlobType::SLOTS_WING);
		} else if (itemTemplate->getItemGroup() == ItemGroup::SHIELD) {
			blob->addBlobEntry(ItemBlobType::SLOTS_SHIELD);
		} else if (itemTemplate->getItemGroup() == ItemGroup::PLUME) {
			blob->addBlobEntry(ItemBlobType::PLUME_INFO);
		} else if (itemTemplate->isArmor()) {
			if (network::detail::isAccessoryArmorGroup(itemTemplate->getItemGroup()))
				blob->addBlobEntry(ItemBlobType::SLOTS_ACCESSORY); // power shards, helmets, earrings, rings, belts
			else
				blob->addBlobEntry(ItemBlobType::SLOTS_ARMOR);
		} else if (itemTemplate->isWeapon()) {
			blob->addBlobEntry(ItemBlobType::SLOTS_WEAPON);
		}

		blob->addBlobEntry(ItemBlobType::ENCHANT_INFO);
		if (itemValue.getConditioningInfo())
			blob->addBlobEntry(ItemBlobType::CONDITIONING_INFO);

		// All items with only General
		if (blob->getBlobEntries().size() > 0) {
			if (itemTemplate->isCanPolish())
				blob->addBlobEntry(ItemBlobType::POLISH_INFO);
			blob->addBlobEntry(ItemBlobType::PREMIUM_OPTION);
		}

		const std::vector<std::unique_ptr<model::stats::calc::functions::StatFunction>>* allModifiers = itemTemplate->getModifiers();
		if (allModifiers != nullptr) {
			for (const std::unique_ptr<model::stats::calc::functions::StatFunction>& modifier : *allModifiers) {
				if (modifier->isBonus() && !modifier->hasConditions()) {
					blob->addBonusBlobEntry(*modifier);
				}
			}
		}
	}
	if (itemTemplate->getItemGroup() == ItemGroup::STIGMA_SHARD)
		blob->addBlobEntry(ItemBlobType::STIGMA_SHARD);

	// GENERAL INFO
	blob->addBlobEntry(ItemBlobType::GENERAL_INFO);

	if (itemValue.getPackCount() != 0) {
		blob->addBlobEntry(ItemBlobType::WRAP_INFO);
	}

	return blob;
}

int32_t ItemInfoBlob::size() {
	int32_t totalSize = 0;
	for (runtime::Ptr<ItemBlobEntry> ent : itemBlobEntries)
		totalSize += ent->getSize() + 1; // 1 C for blob id
	return totalSize;
}

} // namespace aion::gameserver::network::aion::iteminfo
