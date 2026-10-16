#include "aion/gameserver/model/account/PlayerAccountData.h"

#include <utility>

#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/detail/ItemSlotMasks.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/BoundRadius.h"

namespace aion::gameserver::model::account {

PlayerAccountData::VisibleItem::VisibleItem(int8_t slotType, int32_t itemId, int32_t godStoneId, std::optional<int32_t> color)
	: slotType_(slotType), itemId_(itemId), godStoneId_(godStoneId), color_(color) {
}

PlayerAccountData::VisibleItem::~VisibleItem() = default;

runtime::Ref<PlayerAccountData::VisibleItem> PlayerAccountData::VisibleItem::create(int8_t slotType, int32_t itemId, int32_t godStoneId,
	std::optional<int32_t> color) {
	return runtime::makeRef<VisibleItem>(slotType, itemId, godStoneId, color);
}

bool PlayerAccountData::VisibleItem::equals(const VisibleItem& obj) const {
	return this == &obj || (slotType_ == obj.slotType_ && itemId_ == obj.itemId_ && godStoneId_ == obj.godStoneId_ && color_ == obj.color_);
}

int32_t PlayerAccountData::VisibleItem::hashCode() const {
	// Java record hashCode (java.lang.runtime.ObjectMethods): 31 * h + hash(component) over the components in declaration order
	uint32_t h = 0;
	h = 31 * h + static_cast<uint32_t>(slotType_);
	h = 31 * h + static_cast<uint32_t>(itemId_);
	h = 31 * h + static_cast<uint32_t>(godStoneId_);
	h = 31 * h + static_cast<uint32_t>(color_.value_or(0));
	return static_cast<int32_t>(h);
}

PlayerAccountData::PlayerAccountData(Account& account, gameobjects::player::PlayerCommonData& playerCommonDataValue,
	gameobjects::player::PlayerAppearance& appearanceValue)
	: PlayerAccountData(account, playerCommonDataValue, appearanceValue, nullptr, {}) {
	// Java: this(playerCommonData, appearance, null, Collections.emptyList())
}

PlayerAccountData::PlayerAccountData(Account& account, gameobjects::player::PlayerCommonData& playerCommonDataValue,
	gameobjects::player::PlayerAppearance& appearanceValue, runtime::Ptr<CharacterBanInfo> cbiValue,
	std::vector<runtime::Ref<PlayerAccountData::VisibleItem>> visibleItemsValue)
	: OwnedPart(account), playerCommonData(playerCommonDataValue) {
	appearance.set(runtime::Ref<gameobjects::player::PlayerAppearance>(appearanceValue));
	cbi.set(cbiValue);
	runtime::Ref<runtime::RcArrayList<runtime::Ref<VisibleItem>>> items =
		runtime::RcArrayList<runtime::Ref<VisibleItem>>::create(AION_LOCK_CLASS(PlayerAccountData::visibleItems));
	for (runtime::Ref<VisibleItem>& item : visibleItemsValue)
		items->add(std::move(item));
	visibleItems.set(items);
	updateBoundingRadius();
}

PlayerAccountData::~PlayerAccountData() = default;

void PlayerAccountData::setCharBanInfo(runtime::Ptr<CharacterBanInfo> value) {
	cbi.set(value);
}

int32_t PlayerAccountData::getDeletionTimeInSeconds() {
	std::optional<commons::database::Timestamp> date = deletionDate.get();
	return !date ? 0 : static_cast<int32_t>(date->time_since_epoch().count() / 1000);
}

void PlayerAccountData::setAppearance(gameobjects::player::PlayerAppearance& value) {
	appearance.set(runtime::Ptr<gameobjects::player::PlayerAppearance>(value));
	updateBoundingRadius();
}

void PlayerAccountData::updateBoundingRadius() {
	// Java: new BoundRadius(0.25f, 0.25f, appearance.getBoundHeight()); run-time radii are interned immortals (BoundRadius.h)
	playerCommonData->setBoundingRadius(templates::BoundRadius::intern(0.25f, 0.25f, appearance.get()->getBoundHeight()));
}

void PlayerAccountData::setVisibleItems(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment) {
	runtime::Ref<runtime::RcArrayList<runtime::Ref<VisibleItem>>> items =
		runtime::RcArrayList<runtime::Ref<VisibleItem>>::create(AION_LOCK_CLASS(PlayerAccountData::visibleItems));
	for (const runtime::Ptr<gameobjects::Item>& item : equipment) {
		const int8_t slotType = gameobjects::player::detail::getEquipmentSlotType(item->getEquipmentSlot());
		if (slotType != 0)
			items->add(VisibleItem::create(slotType, item->getItemSkinTemplate()->getTemplateId(), item->getGodStoneId(), item->getItemColor()));
	}
	visibleItems.set(items);
}

} // namespace aion::gameserver::model::account
