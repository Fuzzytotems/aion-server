#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"

namespace aion::gameserver::model::gameobjects::player {

PortalCooldownList::PortalCooldownList(Player& ownerValue) : OwnedPart(ownerValue), owner(ownerValue) {
}

PortalCooldownList::~PortalCooldownList() = default;

bool PortalCooldownList::isPortalUseDisabled(int32_t worldId) {
	AION_UNPORTED();
}

int64_t PortalCooldownList::getPortalCooldownTime(int32_t worldId) {
	AION_UNPORTED();
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getPortalCooldown(int32_t worldId) {
	AION_UNPORTED();
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getOrCreatePortalCooldown(int32_t worldId) {
	AION_UNPORTED();
}

runtime::Ptr<PortalCooldown> PortalCooldownList::getOrCreatePortalCooldown(int32_t worldId, int64_t reuseTime) {
	AION_UNPORTED();
}

void PortalCooldownList::setPortalCoolDowns(runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> portalCoolDowns) {
	portalCooldowns.set(portalCoolDowns);
}

void PortalCooldownList::addPortalCooldown(int32_t worldId, int64_t useDelay) {
	AION_UNPORTED();
}

void PortalCooldownList::sendEntryInfo(int32_t worldId) {
	AION_UNPORTED();
}

void PortalCooldownList::removePortalCooldown(int32_t worldId) {
	AION_UNPORTED();
}

bool PortalCooldownList::hasCooldowns() {
	AION_UNPORTED();
}

int32_t PortalCooldownList::size() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
