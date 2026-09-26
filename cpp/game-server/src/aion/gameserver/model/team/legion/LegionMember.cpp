#include "aion/gameserver/model/team/legion/LegionMember.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/team/legion/Legion.h"

namespace aion::gameserver::model::team::legion {

LegionMember::LegionMember(int32_t objectIdValue, Legion& legionValue) : objectId(objectIdValue), legion(legionValue) {
}

LegionMember::~LegionMember() = default;

runtime::Ref<LegionMember> LegionMember::create(int32_t objectIdValue, Legion& legionValue) {
	return runtime::makeRef<LegionMember>(objectIdValue, legionValue);
}

bool LegionMember::isBrigadeGeneral() {
	AION_UNPORTED();
}

void LegionMember::increaseChallengeScore(int32_t amount) {
	AION_UNPORTED();
}

void LegionMember::setPlayerData(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void LegionMember::setPlayerData(gameobjects::player::PlayerCommonData& playerCommonData) {
	AION_UNPORTED();
}

bool LegionMember::hasRights(LegionPermissionsMask permissions) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::legion
