#include "aion/gameserver/model/templates/siegelocation/DoorRepairData.h"

namespace aion::gameserver::model::templates::siegelocation {

void DoorRepairData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const DoorRepairStone& repairStone : doorRepairTemplates)
		doorRepairStones.insertOrAssign(repairStone.staticId, &repairStone);
}

const DoorRepairStone* DoorRepairData::getRepairStone(int32_t stoneStaticId) const {
	const DoorRepairStone* const* stone = doorRepairStones.find(stoneStaticId);
	return stone == nullptr ? nullptr : *stone;
}

std::vector<const DoorRepairStone*> DoorRepairData::getRepairStones() const {
	std::vector<const DoorRepairStone*> stones;
	stones.reserve(doorRepairStones.size());
	for (const auto& [staticId, stone] : doorRepairStones)
		stones.push_back(stone);
	return stones;
}

} // namespace aion::gameserver::model::templates::siegelocation
