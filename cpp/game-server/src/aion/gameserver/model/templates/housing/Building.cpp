#include "aion/gameserver/model/templates/housing/Building.h"

#include <string>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::housing {

namespace {
/** Java DataManager.HOUSE_BUILDING_DATA.getBuilding(id) (NullPointerException if there is none, as Java's field access on the result) */
const Building& holderBuilding(int32_t id) {
	const Building* building = dataholders::DataManager::HOUSE_BUILDING_DATA->getBuilding(id);
	if (building == nullptr)
		throw runtime::NullPointerException("Building " + std::to_string(id) + " is not in the house building data");
	return *building;
}
} // namespace

void Building::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	if (parts == nullptr)
		return;
	partsByType.emplace();
	if (parts->getDoor() != 0)
		partsByType->insertOrAssign(PartType::DOOR, parts->getDoor());
	if (parts->getFence())
		partsByType->insertOrAssign(PartType::FENCE, *parts->getFence());
	if (parts->getFrame())
		partsByType->insertOrAssign(PartType::FRAME, *parts->getFrame());
	if (parts->getGarden())
		partsByType->insertOrAssign(PartType::GARDEN, *parts->getGarden());
	if (parts->getInfloor() != 0)
		partsByType->insertOrAssign(PartType::INFLOOR_ANY, parts->getInfloor());
	if (parts->getInwall() != 0)
		partsByType->insertOrAssign(PartType::INWALL_ANY, parts->getInwall());
	if (parts->getOutwall())
		partsByType->insertOrAssign(PartType::OUTWALL, *parts->getOutwall());
	if (parts->getRoof())
		partsByType->insertOrAssign(PartType::ROOF, *parts->getRoof());
}

const std::string& Building::getPartsMatchTag() const {
	if (partsMatch.empty())
		return holderBuilding(id).partsMatch;
	return partsMatch;
}

std::optional<HouseType> Building::getSize() const {
	if (!size)
		return holderBuilding(id).size;
	return size;
}

std::optional<BuildingType> Building::getType() const {
	if (!type)
		return holderBuilding(id).type;
	return type;
}

std::optional<int32_t> Building::getDefaultDecorId(PartType partType) const {
	const int32_t* decorId = getPartsByType().find(partType);
	return decorId == nullptr ? std::nullopt : std::optional<int32_t>(*decorId);
}

std::vector<int32_t> Building::getDefaultPartIds() const {
	std::vector<int32_t> partIds;
	for (const auto& [partType, partId] : getPartsByType())
		partIds.push_back(partId);
	return partIds;
}

const ::aion::gameserver::model::templates::detail::FlatMap<PartType, int32_t>& Building::getPartsByType() const {
	const std::optional<::aion::gameserver::model::templates::detail::FlatMap<PartType, int32_t>>& byType =
	  partsByType ? partsByType : holderBuilding(id).partsByType;
	if (!byType)
		throw runtime::NullPointerException("Building " + std::to_string(id) + " has no parts");
	return *byType;
}

} // namespace aion::gameserver::model::templates::housing
