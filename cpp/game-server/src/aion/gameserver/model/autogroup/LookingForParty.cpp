#include "aion/gameserver/model/autogroup/LookingForParty.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"

namespace aion::gameserver::model::autogroup {

// Java LookingForParty.java:24-30
LookingForParty::LookingForParty(gameobjects::player::Player& player, EntryRequestType value, int32_t maskIdValue)
	: ert(value), race(player.getRace()), registrationTime(commons::utils::currentTimeMillis()), maskId(maskIdValue) {
	members.putAll(createMembers(player));
	leaderObjId.set(player.getObjectId());
}

runtime::Ref<LookingForParty> LookingForParty::create(gameobjects::player::Player& player, EntryRequestType value, int32_t maskIdValue) {
	return runtime::makeRef<LookingForParty>(player, value, maskIdValue);
}

// Java LookingForParty.java:32-38: the online team members (Collectors.toMap) or the player alone
std::unordered_map<int32_t, runtime::Ref<AGPlayer>> LookingForParty::createMembers(gameobjects::player::Player& player) {
	std::unordered_map<int32_t, runtime::Ref<AGPlayer>> created;
	if (player.isInTeam()) {
		for (const runtime::Ptr<gameobjects::player::Player>& member : player.getCurrentTeam()->getOnlineMembers()) {
			runtime::Ref<AGPlayer> agPlayer = AGPlayer::create(*member);
			created.emplace(agPlayer->objectId(), agPlayer); // Java toMap: a duplicate key would throw IllegalStateException; members are unique
		}
		return created;
	}
	created.emplace(player.getObjectId(), AGPlayer::create(player));
	return created;
}

// Java LookingForParty.java:44-46
bool LookingForParty::isMember(int32_t objectId) {
	return members.get(objectId) != nullptr;
}

// Java LookingForParty.java:48-50 (Integer objectId: a null removes nothing, HashMap.remove(null))
void LookingForParty::unregisterMember(std::optional<int32_t> objectId) {
	if (objectId)
		members.remove(*objectId);
}

// Java LookingForParty.java:76-78
bool LookingForParty::isLeader(int32_t objectId) {
	return objectId == leaderObjId.get();
}

// Java LookingForParty.java:80-82
void LookingForParty::setStartEnterTime() {
	startEnterTime.set(commons::utils::currentTimeMillis());
}

// Java LookingForParty.java:84-86
bool LookingForParty::isOnStartEnterTask() {
	return commons::utils::currentTimeMillis() - startEnterTime.get() <= 120000;
}

// Java LookingForParty.java:88-96: the entry type (higher ordinal first), the larger party, then the earlier registration
int32_t LookingForParty::compareTo(const LookingForParty& lfp) const {
	if (ert != lfp.ert)
		return static_cast<int32_t>(lfp.ert) - static_cast<int32_t>(ert);
	int32_t memberDiff = lfp.members.size() - members.size();
	if (memberDiff != 0)
		return memberDiff;
	return static_cast<int32_t>(registrationTime - lfp.getRegistrationTime()); // Java: (int) of the long difference (wraps)
}

LookingForParty::~LookingForParty() = default;

} // namespace aion::gameserver::model::autogroup
