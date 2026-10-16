#include "aion/gameserver/services/conquerorAndProtectorSystem/CPInfo.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/CPBuff.h"

namespace aion::gameserver::services::conquerorAndProtectorSystem {

CPInfo::CPInfo(model::templates::cp::CPType value, model::gameobjects::player::Player& owner)
	: type(value), playerId(owner.getObjectId()), buff(CPBuff::create()) {
}

runtime::Ref<CPInfo> CPInfo::create(model::templates::cp::CPType value, model::gameobjects::player::Player& owner) {
	return runtime::makeRef<CPInfo>(value, owner);
}

CPInfo::~CPInfo() = default;

} // namespace aion::gameserver::services::conquerorAndProtectorSystem
