#include "aion/gameserver/model/gameobjects/TransformModel.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/TransformType.h"

namespace aion::gameserver::model::gameobjects {

TransformModel::TransformModel(Creature& creature)
	: OwnedPart(creature), owner(creature),
	  originalType(
		  dynamic_cast<player::Player*>(&creature) != nullptr ? skillengine::model::TransformType::PC : skillengine::model::TransformType::NONE),
	  transformType(skillengine::model::TransformType::NONE) {
}

TransformModel::~TransformModel() = default;

void TransformModel::apply(int32_t modelIdValue) {
	AION_UNPORTED();
}

void TransformModel::apply(int32_t modelIdValue, skillengine::model::TransformType type, int32_t panelIdValue, bool cantUseSkills, bool cantMove,
	bool cantRecall, bool cantJump, bool cantAttack, bool cantUseItems, bool cantFly) {
	AION_UNPORTED();
}

void TransformModel::updateVisually() {
	AION_UNPORTED();
}

void TransformModel::updateTribeVisually() {
	AION_UNPORTED();
}

int32_t TransformModel::getModelId() {
	AION_UNPORTED();
}

bool TransformModel::isUnrestricted() {
	AION_UNPORTED();
}

bool TransformModel::isActive() {
	AION_UNPORTED();
}

void TransformModel::setTribe(std::optional<TribeClass> value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects
