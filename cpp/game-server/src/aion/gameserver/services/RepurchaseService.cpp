#include "aion/gameserver/services/RepurchaseService.h"

#include <algorithm>
#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::services {

RepurchaseService::RepurchaseService() = default;

void RepurchaseService::addRepurchaseItems(model::gameobjects::player::Player& player,
	const std::vector<runtime::Ptr<model::gameobjects::Item>>& items) {
	runtime::Ref<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>> itemSet =
		runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>::create(AION_LOCK_CLASS(RepurchaseService::repurchaseItems#value));
	for (const runtime::Ptr<model::gameobjects::Item>& item : items)
		itemSet->add(runtime::Ref<model::gameobjects::Item>(*item));
	repurchaseItems.put(player.getObjectId(), std::move(itemSet));
}

void RepurchaseService::removeRepurchaseItems(model::gameobjects::player::Player& player) {
	repurchaseItems.remove(player.getObjectId());
}

std::unordered_set<runtime::Ptr<model::gameobjects::Item>> RepurchaseService::getRepurchaseItems(int32_t playerObjectId) {
	std::unordered_set<runtime::Ptr<model::gameobjects::Item>> result; // Java: getOrDefault(playerObjectId, Collections.emptySet())
	if (runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::gameobjects::Item>>> items = repurchaseItems.get(playerObjectId)) {
		for (const runtime::Ptr<model::gameobjects::Item>& item : items->snapshot())
			result.insert(item);
	}
	return result;
}

bool RepurchaseService::canRepurchase(model::gameobjects::player::Player& player, int32_t itemObjectId) {
	return std::ranges::any_of(getRepurchaseItems(player.getObjectId()),
		[itemObjectId](const runtime::Ptr<model::gameobjects::Item>& item) { return item->getObjectId() == itemObjectId; });
}

void RepurchaseService::repurchaseFromShop(model::gameobjects::player::Player& player, model::trade::RepurchaseList& repurchaseList) {
	AION_UNPORTED();
}

RepurchaseService& RepurchaseService::getInstance() {
	static RepurchaseService instance; // Java SingletonHolder
	return instance;
}

} // namespace aion::gameserver::services
