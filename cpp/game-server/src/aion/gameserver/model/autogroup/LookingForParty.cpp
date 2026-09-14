#include "aion/gameserver/model/autogroup/LookingForParty.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"

namespace aion::gameserver::model::autogroup {

LookingForParty::LookingForParty(gameobjects::player::Player& player, EntryRequestType value, int32_t maskIdValue)
	: ert(value), race(), registrationTime(commons::utils::currentTimeMillis()), maskId(maskIdValue) {
	// Java: this.members = createMembers(player); this.race = player.getRace(); this.leaderObjId = player.getObjectId()
	AION_UNPORTED();
}

runtime::Ref<LookingForParty> LookingForParty::create(gameobjects::player::Player& player, EntryRequestType value, int32_t maskIdValue) {
	return runtime::makeRef<LookingForParty>(player, value, maskIdValue);
}

std::unordered_map<int32_t, runtime::Ref<AGPlayer>> LookingForParty::createMembers(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool LookingForParty::isMember(int32_t objectId) {
	AION_UNPORTED();
}

void LookingForParty::unregisterMember(std::optional<int32_t> objectId) {
	AION_UNPORTED();
}

bool LookingForParty::isLeader(int32_t objectId) {
	AION_UNPORTED();
}

void LookingForParty::setStartEnterTime() {
	AION_UNPORTED();
}

bool LookingForParty::isOnStartEnterTask() {
	AION_UNPORTED();
}

int32_t LookingForParty::compareTo(const LookingForParty& lfp) const {
	AION_UNPORTED();
}

LookingForParty::~LookingForParty() = default;

} // namespace aion::gameserver::model::autogroup
