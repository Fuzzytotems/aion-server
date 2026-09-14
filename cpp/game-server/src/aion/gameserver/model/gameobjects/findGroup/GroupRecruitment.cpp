#include "aion/gameserver/model/gameobjects/findGroup/GroupRecruitment.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"

namespace aion::gameserver::model::gameobjects::findGroup {

GroupRecruitment::GroupRecruitment(AionObject& value, std::string_view messageValue, int32_t groupTypeValue)
	: object(runtime::Ref<AionObject>(value)), message(std::string(messageValue)), groupType(groupTypeValue) {
}

runtime::Ref<GroupRecruitment> GroupRecruitment::create(AionObject& value, std::string_view messageValue, int32_t groupTypeValue) {
	return runtime::makeRef<GroupRecruitment>(value, messageValue, groupTypeValue);
}

int32_t GroupRecruitment::getObjectId() {
	AION_UNPORTED();
}

int32_t GroupRecruitment::getClassId() {
	AION_UNPORTED();
}

int32_t GroupRecruitment::getMinLevel() {
	AION_UNPORTED();
}

int32_t GroupRecruitment::getMaxLevel() {
	AION_UNPORTED();
}

std::string GroupRecruitment::getName() {
	AION_UNPORTED();
}

int32_t GroupRecruitment::getSize() {
	AION_UNPORTED();
}

void GroupRecruitment::updateLastUpdate() {
	AION_UNPORTED();
}

Race GroupRecruitment::getRace() {
	AION_UNPORTED();
}

GroupRecruitment::~GroupRecruitment() = default;

} // namespace aion::gameserver::model::gameobjects::findGroup
