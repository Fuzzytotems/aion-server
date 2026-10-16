#include "aion/gameserver/dataholders/PlayerInitialData.h"

#include <format>

#include "aion/gameserver/dataholders/PlayerInitialData_PlayerCreationData_ItemsType.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

void PlayerInitialData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const PlayerCreationData& pt : dataList)
		data.insert_or_assign(pt.getRequiredPlayerClass(), &pt);
	// Java: dataList = null (the C++ index points into the storage, which stays)
}

const PlayerInitialData::PlayerCreationData* PlayerInitialData::getPlayerCreationData(model::PlayerClass cls) const {
	auto it = data.find(cls);
	return it != data.end() ? it->second : nullptr;
}

int32_t PlayerInitialData::size() const {
	return static_cast<int32_t>(data.size());
}

const PlayerInitialData::LocationData& PlayerInitialData::getSpawnLocation(model::Race race) const {
	switch (race) {
		case model::Race::ASMODIANS:
			return *asmodianSpawnLocation; // required element
		case model::Race::ELYOS:
			return *elyosSpawnLocation; // required element
		default:
			throw runtime::IllegalArgumentException("");
	}
}

const std::vector<PlayerInitialData::PlayerCreationData::ItemType>& PlayerInitialData::PlayerCreationData::getItems() const {
	if (itemsType == nullptr)
		throw runtime::NullPointerException("Cannot read field \"items\" because \"this.itemsType\" is null");
	return itemsType->items;
}

std::string PlayerInitialData::PlayerCreationData::ItemType::toString() const {
	std::string templateText = template_ == nullptr
	                             ? "null"
	                             : std::format("com.aionemu.gameserver.model.templates.item.ItemTemplate@{:x}", reinterpret_cast<uintptr_t>(template_));
	return "ItemType{template=" + templateText + ", count=" + std::to_string(count) + "}";
}

} // namespace aion::gameserver::dataholders
