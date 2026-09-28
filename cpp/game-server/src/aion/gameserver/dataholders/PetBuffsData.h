#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/gameserver/dataholders/PetBuffsData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PetBuffsData.
 * <p>
 * C++: the index points into the bound `buffs` storage, which stays (Java clears the list). Java's LinkedHashMap order is not observable: the
 * holder only looks buffs up.
 *
 * @author Rolandas
 */
class PetBuffsData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetBuffsData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::pet::PetBuff*> petBuffsById;

public:
	/** @return the pet buff, nullptr (Java null) if there is none */
	const model::templates::pet::PetBuff* getPetBuff(int32_t buffId) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
