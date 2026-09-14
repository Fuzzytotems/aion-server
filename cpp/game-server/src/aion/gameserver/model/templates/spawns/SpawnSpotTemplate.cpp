#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::spawns {

SpawnSpotTemplate::SpawnSpotTemplate(float xValue, float yValue, float zValue, int8_t hValue, int32_t randomWalkValue,
	std::optional<std::string_view> walkerIdValue, std::optional<int32_t> walkerIndex)
	: x(xValue), y(yValue), z(zValue), h(hValue), randomWalk(randomWalkValue > 0 ? randomWalkValue : 0),
	  walkerId(walkerIdValue ? std::string(*walkerIdValue) : std::string()), walkerIdx(walkerIndex) {
}

void SpawnSpotTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::spawns
