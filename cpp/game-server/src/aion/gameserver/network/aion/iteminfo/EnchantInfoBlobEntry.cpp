#include "aion/gameserver/network/aion/iteminfo/EnchantInfoBlobEntry.h"

#include <algorithm>
#include <optional>
#include <string>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/IdianStone.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

using model::gameobjects::Item;
using model::stats::container::PlumStatEnum;

EnchantInfoBlobEntry::EnchantInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::ENCHANT_INFO) {
}

EnchantInfoBlobEntry::~EnchantInfoBlobEntry() = default;

runtime::Ref<EnchantInfoBlobEntry> EnchantInfoBlobEntry::create() {
	return runtime::makeRef<EnchantInfoBlobEntry>();
}

void EnchantInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeInfo(buf, *ownerItem.get());
}

void EnchantInfoBlobEntry::writeInfo(commons::utils::ByteBuffer& buf, Item& item) {
	writeInfo(buf, item, !item.isIdentified() ? -1 : item.getOptionalSockets(), !item.isIdentified() ? -1 : item.getEnchantBonus());
}

void EnchantInfoBlobEntry::writeInfo(commons::utils::ByteBuffer& buf, Item& item, int32_t optionalManastoneSockets, int32_t enchantBonus) {
	writeC(buf, item.isSoulBound() ? 1 : 0);
	writeC(buf, item.getEnchantLevel()); // enchant (1-15)
	writeD(buf, item.getItemSkinTemplate()->getTemplateId());
	writeC(buf, optionalManastoneSockets);
	writeC(buf, enchantBonus);
	std::unordered_map<int32_t, runtime::Ptr<model::items::ManaStone>> stonesBySlot = createManastoneMap(item);
	for (int32_t i = 0; i < Item::MAX_BASIC_STONES; i++) {
		auto stone = stonesBySlot.find(i);
		writeD(buf, stone == stonesBySlot.end() ? 0 : stone->second->getItemId());
	}
	writeD(buf, item.getGodStoneId());
	int32_t dyeExpiration = item.getColorTimeLeft();
	writeDyeInfo(buf, dyeExpiration < 0 ? std::nullopt : item.getItemColor());
	writeC(buf, 0); // unk (0)
	writeD(buf, 0); // unk 1.5.1.9
	writeD(buf, std::max(0, dyeExpiration)); // seconds until dye expires
	runtime::Ptr<model::items::IdianStone> idianStone = item.getIdianStone();
	if (idianStone && idianStone->getPolishNumber() > 0) {
		writeD(buf, idianStone->getItemId()); // Idian Stone template ID
		writeC(buf, idianStone->getPolishNumber()); // polish statset ID
	} else {
		writeD(buf, 0); // Idian Stone template ID
		writeC(buf, 0); // polish statset ID
	}
	writeC(buf, item.getTempering()); // tempering level
	writeD(buf, 0x00);
	writeC(buf, 0x00);
	writeD(buf, 0x00);
	writeC(buf, 0x00);
	writeD(buf, 0x00);
	writeD(buf, 0x00);
	if (item.getTempering() > 0 && item.getItemTemplate()->getItemGroup() == model::templates::item::enums::ItemGroup::PLUME) {
		PlumStatEnum stat = item.getItemTemplate()->getTemperingName() == "TSHIRT_PHYSICAL" ? PlumStatEnum::PLUM_PHISICAL_ATTACK
																																														: PlumStatEnum::PLUM_BOOST_MAGICAL_SKILL;
		writeD(buf, network::detail::plumStatIdOf(PlumStatEnum::PLUM_HP)); // 1st satId
		writeD(buf, network::detail::plumStatBoostValueOf(PlumStatEnum::PLUM_HP) * item.getTempering()); // value
		writeD(buf, network::detail::plumStatIdOf(stat)); // 2nd statId
		writeD(buf, (network::detail::plumStatBoostValueOf(stat) * item.getTempering()) + item.getRndPlumeBonusValue()); // value
	} else {
		writeD(buf, 0x00); // 1st statId
		writeD(buf, 0x00); // value
		writeD(buf, 0x00); // 2nd statId
		writeD(buf, 0x00); // value
	}
	writeD(buf, 0x00); // 3rd statId
	writeD(buf, 0x00);
	writeD(buf, 0x00); // 4th statId
	writeD(buf, 0x00);
	writeD(buf, 0x00); // 5th statId
	writeD(buf, 0x00);
	writeD(buf, 0x00); // 6th statId
	writeD(buf, 0x00);
	writeD(buf, 0x00); // unk 4.7.5
	writeC(buf, item.isAmplified() ? 1 : 0);
	writeD(buf, item.getBuffSkill());
	writeD(buf, 0x00); // skillId
	writeD(buf, 0x00); // skillId
}

std::unordered_map<int32_t, runtime::Ptr<model::items::ManaStone>> EnchantInfoBlobEntry::createManastoneMap(Item& item) {
	std::unordered_map<int32_t, runtime::Ptr<model::items::ManaStone>> stonesBySlot;
	if (item.hasManaStones()) {
		for (runtime::Ptr<model::items::ManaStone> stone : *item.getItemStones()) {
			if (!stonesBySlot.emplace(stone->getSlot(), stone).second)
				throw commons::utils::IllegalStateException("Duplicate key " + std::to_string(stone->getSlot())); // Java: Collectors.toMap
		}
	}
	return stonesBySlot;
}

int32_t EnchantInfoBlobEntry::getSize() {
	return SIZE;
}

} // namespace aion::gameserver::network::aion::iteminfo
