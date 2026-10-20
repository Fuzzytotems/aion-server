#include "aion/gameserver/handlers/ai/events/HalloweenPumpkinAI.h"

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/drop/DropItem.h"
#include "aion/gameserver/model/gameobjects/DropNpc.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/services/drop/DropRegistrationService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::handlers::ai::events {

AION_AI(HalloweenPumpkinAI, "halloween_pumpkin");

runtime::ConcurrentHashMap<int32_t, int64_t>& HalloweenPumpkinAI::nextRewardMillisByAccountId() {
	static auto* const map = new runtime::ConcurrentHashMap<int32_t, int64_t>(AION_LOCK_CLASS(HalloweenPumpkinAI::nextRewardMillisByAccountId#stripe));
	return *map;
}

// Java HalloweenPumpkinAI.java:39-46
void HalloweenPumpkinAI::handleSpawned() {
	if (getNpcId() != 219484 && commons::utils::Rnd::chance() < 33) {
		replaceWithLargePumpkin();
		return;
	}
	OneDmgAI::handleSpawned();
}

// Java HalloweenPumpkinAI.java:48-51
bool HalloweenPumpkinAI::canThink() {
	return false;
}

// Java HalloweenPumpkinAI.java:53-57
void HalloweenPumpkinAI::modifyOwnerStat(Stat2& stat) {
	if (stat.getStat() == model::stats::container::StatEnum::MAXHP)
		stat.setBase(10);
}

// Java HalloweenPumpkinAI.java:59-68
void HalloweenPumpkinAI::handleDropRegistered() {
	if (getNpcId() == 219484) {
		float chance = commons::utils::Rnd::chance();
		runtime::Ptr<Player> p = getAggroList().getMostPlayerDamage();
		if (chance < 1 && p != nullptr && canReward(*p))
			replaceDropWithItem(162000131); // Fine Bracing Water
		else if (chance < 3)
			tryReplaceDropWithWeaponPrototype();
	}
}

// Java HalloweenPumpkinAI.java:70-81
bool HalloweenPumpkinAI::canReward(Player& player) {
	runtime::AtomicBoolean canReward{AION_LOCK_CLASS(HalloweenPumpkinAI::canReward)};
	nextRewardMillisByAccountId().compute(player.getAccount()->getId(),
		[&canReward](const std::optional<int64_t>& nextRewardTime) -> std::optional<int64_t> {
		int64_t nowMs = commons::utils::currentTimeMillis();
		std::optional<int64_t> next = nextRewardTime;
		if (!next.has_value() || nowMs > *next) {
			next = nowMs + 24LL * 60 * 60 * 1000; // TimeUnit.DAYS.toMillis(1)
			canReward.set(true);
		}
		return next;
	});
	return canReward.get();
}

// Java HalloweenPumpkinAI.java:83-89
void HalloweenPumpkinAI::replaceDropWithItem(int32_t itemId) {
	runtime::Ptr<runtime::RcHashSet<runtime::Ref<model::drop::DropItem>>> drops =
		services::drop::DropRegistrationService::getInstance().getCurrentDropMap().get(getObjectId());
	if (drops == nullptr)
		return;
	drops->clear();
	drops->add(services::drop::DropRegistrationService::getInstance().regDropItem(1, 0, getObjectId(), itemId, 1));
}

// Java HalloweenPumpkinAI.java:91-98
void HalloweenPumpkinAI::tryReplaceDropWithWeaponPrototype() {
	runtime::Ptr<model::gameobjects::DropNpc> dropNpc =
		services::drop::DropRegistrationService::getInstance().getDropRegistrationMap().get(getObjectId());
	if (dropNpc == nullptr)
		return;
	std::optional<int32_t> prototypeItemId = selectWeaponPrototypeItemId(*dropNpc->getAllowedLooters());
	if (prototypeItemId.has_value())
		replaceDropWithItem(*prototypeItemId);
}

// Java HalloweenPumpkinAI.java:100-107
std::optional<int32_t> HalloweenPumpkinAI::selectWeaponPrototypeItemId(runtime::RcHashSet<int32_t>& allowedLooters) {
	std::vector<int32_t> itemIds;
	for (int32_t id : allowedLooters.snapshot()) {
		if (runtime::Ptr<Player> player = runtime::as<Player>(getKnownList().getObject(id))) {
			for (int32_t itemId : getWeaponPrototypeItemIds(*player))
				itemIds.push_back(itemId);
		}
	}
	return commons::utils::Rnd::getOptional(itemIds);
}

// Java HalloweenPumpkinAI.java:109-117
std::vector<int32_t> HalloweenPumpkinAI::getWeaponPrototypeItemIds(Player& player) {
	switch (player.getPlayerClass()) {
		case PlayerClass::GLADIATOR:
			return {169405391, 169405395}; // Upgraded Distorted Sword/Polearm of the Fierce Battle Prototype
		case PlayerClass::TEMPLAR:
			return {169405391, 169405394}; // Upgraded Distorted Sword/Greatsword of the Fierce Battle Prototype
		case PlayerClass::ASSASSIN:
		case PlayerClass::RANGER:
			return {169405391, 169405393}; // Upgraded Distorted Sword/Dagger of the Fierce Battle Prototype
		case PlayerClass::CLERIC:
		case PlayerClass::CHANTER:
			return {169405392, 169405396}; // Upgraded Distorted Mace/Staff of the Fierce Battle Prototype
		default:
			return {};
	}
}

// Java HalloweenPumpkinAI.java:119-128
void HalloweenPumpkinAI::replaceWithLargePumpkin() {
	getOwner().getController().delete_();
	runtime::Ptr<SpawnTemplate> spot = getSpawnTemplate();
	if (getSpawnTemplate()->hasPool()) // despawning frees its pool spot, so we cannot reuse it
		spot = getSpawnTemplate()->getGroup().reserveRandomFreePoolSpot(getOwner().getInstanceId());
	runtime::Ref<Npc> largePumpkin = VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), *spot,
		dataholders::DataManager::NPC_DATA->getNpcTemplate(219484));
	largePumpkin->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*largePumpkin));
	largePumpkin->setEffectController(std::make_unique<controllers::effect::EffectController>(*largePumpkin));
	spawnengine::SpawnEngine::bringIntoWorld(*largePumpkin, spot->getWorldId(), getOwner().getInstanceId(), spot->getX(), spot->getY(), spot->getZ(),
		spot->getHeading());
}

} // namespace aion::gameserver::handlers::ai::events
