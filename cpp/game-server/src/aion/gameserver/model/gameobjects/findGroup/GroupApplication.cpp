#include "aion/gameserver/model/gameobjects/findGroup/GroupApplication.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::gameobjects::findGroup {

GroupApplication::GroupApplication(gameobjects::player::Player& value, std::string_view messageValue, int32_t groupTypeValue, int32_t classIdValue,
	int32_t levelValue)
	: player(runtime::Ref<gameobjects::player::Player>(value)), message(std::string(messageValue)), groupType(groupTypeValue), classId(classIdValue),
	  level(levelValue) {
}

runtime::Ref<GroupApplication> GroupApplication::create(gameobjects::player::Player& value, std::string_view messageValue, int32_t groupTypeValue,
	int32_t classIdValue, int32_t levelValue) {
	return runtime::makeRef<GroupApplication>(value, messageValue, groupTypeValue, classIdValue, levelValue);
}

void GroupApplication::updateLastUpdate() {
	AION_UNPORTED();
}

GroupApplication::~GroupApplication() = default;

} // namespace aion::gameserver::model::gameobjects::findGroup
