#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/PortalCooldownsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_INFO.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::model::gameobjects::player {

PortalCooldownList::PortalCooldownList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

PortalCooldownList::~PortalCooldownList() = default;

bool PortalCooldownList::isPortalUseDisabled(int32_t worldId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	if (!cooldowns || !cooldowns->containsKey(worldId))
		return false;

	runtime::Ptr<PortalCooldown> coolDown = cooldowns->get(worldId);
	if (!coolDown)
		return false;

	if (coolDown->getReuseTime() < commons::utils::currentTimeMillis()) {
		cooldowns->remove(worldId);
		return false;
	}

	return coolDown->getEnterCount() >= dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceMaxCountByWorldId(worldId);
}

int64_t PortalCooldownList::getPortalCooldownTime(int32_t worldId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	if (!cooldowns || !cooldowns->containsKey(worldId))
		return 0;
	int64_t coolDown = cooldowns->get(worldId)->getReuseTime();

	if (coolDown < commons::utils::currentTimeMillis()) {
		cooldowns->remove(worldId);
		return 0;
	}

	return coolDown;
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getPortalCooldown(int32_t worldId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	return !cooldowns ? nullptr : cooldowns->get(worldId);
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getOrCreatePortalCooldown(int32_t worldId) {
	int64_t reuseTime = dataholders::DataManager::INSTANCE_COOLTIME_DATA->calculateInstanceEntranceCooltime(owner, worldId);
	if (reuseTime == 0)
		return nullptr;
	return getOrCreatePortalCooldown(worldId, reuseTime);
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getOrCreatePortalCooldown(int32_t worldId, int64_t reuseTime) {
	SYNCHRONIZED(*this) {
		if (!portalCooldowns)
			portalCooldowns.set(runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>::create(AION_LOCK_CLASS(PortalCooldownList::portalCooldowns)));
		return portalCooldowns->computeIfAbsent(worldId, [reuseTime](int32_t id) { return PortalCooldown::create(id, reuseTime, 0); });
	}
}

void PortalCooldownList::setPortalCoolDowns(runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> portalCoolDowns) {
	portalCooldowns.set(portalCoolDowns);
}

void PortalCooldownList::addPortalCooldown(int32_t worldId, int64_t useDelay) {
	getOrCreatePortalCooldown(worldId, useDelay)->increaseEnterCount();

	dao::PortalCooldownsDAO::storePortalCooldowns(owner);

	sendEntryInfo(worldId);
}

void PortalCooldownList::sendEntryInfo(int32_t worldId) {
	if (owner.isInTeam()) {
		network::aion::serverpackets::SM_INSTANCE_INFO packet(static_cast<int8_t>(2), owner, {worldId});
		owner.getCurrentTeam()->sendPackets({packet});
	} else {
		utils::PacketSendUtility::sendPacket(owner, network::aion::serverpackets::SM_INSTANCE_INFO(static_cast<int8_t>(2), owner, {worldId}));
	}
}

void PortalCooldownList::removePortalCooldown(int32_t worldId) {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	if (cooldowns)
		cooldowns->remove(worldId);
}

bool PortalCooldownList::hasCooldowns() {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	return cooldowns && cooldowns->size() > 0;
}

int32_t PortalCooldownList::size() {
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> cooldowns = portalCooldowns.get();
	return cooldowns ? cooldowns->size() : 0;
}

} // namespace aion::gameserver::model::gameobjects::player
