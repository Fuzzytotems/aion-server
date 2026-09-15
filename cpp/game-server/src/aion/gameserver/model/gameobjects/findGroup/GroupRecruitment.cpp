#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::findGroup {

GroupRecruitment::GroupRecruitment(AionObject& value, std::string_view messageValue, int32_t groupTypeValue)
	: object(runtime::Ref<AionObject>(value)), message(std::string(messageValue)), groupType(groupTypeValue),
	  lastUpdate(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000)) {
}

runtime::Ref<GroupRecruitment> GroupRecruitment::create(AionObject& value, std::string_view messageValue, int32_t groupTypeValue) {
	return runtime::makeRef<GroupRecruitment>(value, messageValue, groupTypeValue);
}

int32_t GroupRecruitment::getObjectId() {
	return object->getObjectId();
}

int32_t GroupRecruitment::getClassId() {
	if (classId.get() != -1)
		return classId.get();
	if (runtime::Ptr<player::Player> player = runtime::as<player::Player>(runtime::Ptr<AionObject>(object)))
		return model::getClassId(player->getPlayerClass());
	if (runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object)))
		return model::getClassId(team->getLeaderObject()->getPlayerClass());
	return 0;
}

int32_t GroupRecruitment::getMinLevel() {
	if (level.get() != -1)
		return level.get();
	if (runtime::Ptr<player::Player> player = runtime::as<player::Player>(runtime::Ptr<AionObject>(object)))
		return player->getLevel();
	if (runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object)))
		return team->getMinExpPlayerLevel();
	return 1;
}

int32_t GroupRecruitment::getMaxLevel() {
	if (runtime::Ptr<player::Player> player = runtime::as<player::Player>(runtime::Ptr<AionObject>(object)))
		return player->getLevel();
	else if (runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object)))
		return team->getMaxExpPlayerLevel();
	return 1;
}

std::string GroupRecruitment::getName() {
	runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object));
	return team ? team->getLeaderObject()->getName(true) : runtime::cast<player::Player>(runtime::Ptr<AionObject>(object))->getName(true);
}

int32_t GroupRecruitment::getSize() {
	runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object));
	return team ? team->size() : 1;
}

void GroupRecruitment::updateLastUpdate() {
	lastUpdate.set(static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000));
}

Race GroupRecruitment::getRace() {
	if (runtime::Ptr<player::Player> player = runtime::as<player::Player>(runtime::Ptr<AionObject>(object)))
		return player->getRace();
	if (runtime::Ptr<team::TemporaryPlayerTeam> team = runtime::as<team::TemporaryPlayerTeam>(runtime::Ptr<AionObject>(object)))
		return team->getRace();
	// Java returns null for other objects; FindGroupService registers only players and teams
	throw runtime::NullPointerException("GroupRecruitment.getRace: neither a Player nor a TemporaryPlayerTeam");
}

GroupRecruitment::~GroupRecruitment() = default;

} // namespace aion::gameserver::model::gameobjects::findGroup
