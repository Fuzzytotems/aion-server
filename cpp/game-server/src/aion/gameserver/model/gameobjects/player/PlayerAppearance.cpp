#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

PlayerAppearance::PlayerAppearance() = default;

PlayerAppearance::~PlayerAppearance() = default;

runtime::Ref<PlayerAppearance> PlayerAppearance::create() {
	return runtime::makeRef<PlayerAppearance>();
}

float PlayerAppearance::getBoundHeight() {
	return height.get() * 1.75f;
}

} // namespace aion::gameserver::model::gameobjects::player
