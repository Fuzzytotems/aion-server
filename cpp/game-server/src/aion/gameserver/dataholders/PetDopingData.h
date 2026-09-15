#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/PetDopingData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PetDopingData.
 * <p>
 * C++: the index points into the bound `list` storage, which stays after afterUnmarshal (static-data.md §2.6).
 *
 * @author Rolandas
 */
class PetDopingData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetDopingData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::pet::PetDopingEntry*> dopingsById;

public:
	int32_t size() const;

	/** @return the doping template, nullptr (Java null) if there is none */
	const model::templates::pet::PetDopingEntry* getDopingTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
