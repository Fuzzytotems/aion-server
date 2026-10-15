#include "aion/gameserver/model/autogroup/AutoInstance.h"

#include <algorithm>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/EntryRequestType.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::autogroup {

using gameobjects::player::Player;

AutoInstance::AutoInstance(AutoGroupType value)
	: agt(value) {
}

// Java AutoInstance.java:32-47: the items of the id, those that expire first used first (List.sort is stable)
bool AutoInstance::removeItem(Player& player, int32_t itemId, int64_t requiredCount) {
	int64_t itemCount = 0;
	std::vector<runtime::Ptr<gameobjects::Item>> items = player.getInventory().getItemsByItemId(itemId);
	for (const runtime::Ptr<gameobjects::Item>& item : items)
		itemCount += item->getItemCount();
	if (itemCount < requiredCount)
		return false;
	std::ranges::stable_sort(items, [](const runtime::Ptr<gameobjects::Item>& a, const runtime::Ptr<gameobjects::Item>& b) {
		return a->getExpireTime() < b->getExpireTime();
	});
	for (const runtime::Ptr<gameobjects::Item>& item : items) {
		requiredCount = player.getInventory().decreaseItemCount(*item, requiredCount);
		if (requiredCount == 0)
			break;
	}
	return true;
}

// Java AutoInstance.java:48-52
void AutoInstance::onInstanceCreate(world::WorldMapInstance& value) {
	this->instance.set(runtime::Ref<world::WorldMapInstance>(value));
	startInstanceTime.set(commons::utils::currentTimeMillis());
}

// Java AutoInstance.java:54-57
AGQuestion AutoInstance::addLookingForParty(LookingForParty& /*lookingForParty*/) {
	return AGQuestion::FAILED;
}

// Java AutoInstance.java:59-61
void AutoInstance::onEnterInstance(Player& /*player*/) {
}

// Java AutoInstance.java:63-65
void AutoInstance::onLeaveInstance(Player& /*player*/) {
}

// Java AutoInstance.java:67-72
void AutoInstance::onPressEnter(Player& player) {
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance == nullptr) // Java: instance.getMapId() on a null instance
		throw runtime::NullPointerException("AutoInstance.instance is null");
	int64_t instanceCoolTime = dataholders::DataManager::INSTANCE_COOLTIME_DATA->calculateInstanceEntranceCooltime(player, mapInstance->getMapId());
	if (instanceCoolTime > 0)
		player.getPortalCooldownList().addPortalCooldown(mapInstance->getMapId(), instanceCoolTime);
}

// Java AutoInstance.java:74-77
void AutoInstance::unregister(Player& player) {
	registeredAGPlayers.remove(player.getObjectId());
}

// Java AutoInstance.java:79-90
bool AutoInstance::isRegistrationDisabled(LookingForParty& lfp) {
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance != nullptr) {
		runtime::Ptr<model::instance::instancescore::InstanceScore> instanceScore = mapInstance->getInstanceHandler()->getInstanceScore();
		if (instanceScore != nullptr && instanceScore->getInstanceProgressionType() == model::instance::InstanceProgressionType::END_PROGRESS)
			return true;
	}
	if (startInstanceTime.get() == 0)
		return false;
	else if (lfp.getEntryRequestType() == EntryRequestType::QUICK_GROUP_ENTRY)
		return commons::utils::currentTimeMillis() - startInstanceTime.get() > getMaximumJoinTime(agt);
	return true;
}

// Java AutoInstance.java:108-110
std::vector<runtime::Ptr<AGPlayer>> AutoInstance::getAGPlayersByRace(Race race) {
	std::vector<runtime::Ptr<AGPlayer>> result;
	for (const runtime::Ref<AGPlayer>& p : registeredAGPlayers.values())
		if (p->race() == race)
			result.push_back(runtime::Ptr<AGPlayer>(p));
	return result;
}

// Java AutoInstance.java:112-114
std::vector<runtime::Ptr<AGPlayer>> AutoInstance::getAGPlayersByClass(PlayerClass playerClass) {
	std::vector<runtime::Ptr<AGPlayer>> result;
	for (const runtime::Ref<AGPlayer>& p : registeredAGPlayers.values())
		if (p->playerClass() == playerClass)
			result.push_back(runtime::Ptr<AGPlayer>(p));
	return result;
}

// Java AutoInstance.java:116-118
std::vector<runtime::Ptr<Player>> AutoInstance::getPlayersByRace(Race race) {
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance == nullptr) // Java: instance.getPlayersInside() on a null instance
		throw runtime::NullPointerException("AutoInstance.instance is null");
	std::vector<runtime::Ptr<Player>> result;
	for (const runtime::Ptr<Player>& p : mapInstance->getPlayersInside())
		if (p->getRace() == race)
			result.push_back(p);
	return result;
}

// Java AutoInstance.java:120-125. C++: "the first registered player" is the first of the ConcurrentHashMap's iteration, whose order differs
// from Java's (P5-10.md); it only picks the race whose member count the cooltime data gives.
int32_t AutoInstance::getMaxPlayers() {
	runtime::Ref<world::WorldMapInstance> mapInstance = instance.get();
	if (mapInstance != nullptr)
		return mapInstance->getMaxPlayers();
	Race race = Race::ELYOS;
	if (!registeredAGPlayers.isEmpty()) {
		const std::vector<runtime::Ptr<AGPlayer>> registered = registeredAGPlayers.values(); // Java: values().iterator().next()
		if (!registered.empty())
			race = registered.front()->race();
	}
	const AutoGroup* found = getTemplate(agt);
	if (found == nullptr) // Java: agt.getTemplate().getInstanceMapId() of a missing template
		throw runtime::NullPointerException("AutoGroupType without auto group template");
	return dataholders::DataManager::INSTANCE_COOLTIME_DATA->getMaxMemberCount(found->getInstanceMapId(), race);
}

AutoInstance::~AutoInstance() = default;

} // namespace aion::gameserver::model::autogroup
