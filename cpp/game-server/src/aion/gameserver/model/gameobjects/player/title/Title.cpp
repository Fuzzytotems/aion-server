#include "aion/gameserver/model/gameobjects/player/title/Title.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player::title {

Title::Title(const templates::TitleTemplate* templateValue, int32_t idValue, int32_t expireTimeValue)
	: template_(templateValue), id(idValue), expireTime(expireTimeValue) {
}

Title::~Title() = default;

runtime::Ref<Title> Title::create(const templates::TitleTemplate* templateValue, int32_t idValue, int32_t expireTimeValue) {
	return runtime::makeRef<Title>(templateValue, idValue, expireTimeValue);
}

void Title::onExpire(Player& player) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::title
