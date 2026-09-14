#include "aion/gameserver/model/gameobjects/player/InRoll.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::gameobjects::player {

InRoll::InRoll(int32_t npcIdValue, int32_t itemIdValue, int32_t indexValue, int32_t rollTypeValue)
	: npcId(npcIdValue), itemId(itemIdValue), rollType(rollTypeValue), index(indexValue) {
}

InRoll::~InRoll() = default;

runtime::Ref<InRoll> InRoll::create(int32_t npcIdValue, int32_t itemIdValue, int32_t indexValue, int32_t rollTypeValue) {
	return runtime::makeRef<InRoll>(npcIdValue, itemIdValue, indexValue, rollTypeValue);
}

void InRoll::setIndexd(int32_t value) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player
