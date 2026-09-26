#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/PetData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PetData.
 * <p>
 * C++: the index points into the bound `pets` storage, which stays after afterUnmarshal (static-data.md §2.6). getPetIds returns the ids in
 * Java's HashMap<Integer, PetTemplate> iteration order.
 *
 * @author IlBuono
 */
class PetData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::pet::PetTemplate*> petData;
	/** C++ only: petData.keySet() in Java's HashMap iteration order */
	std::vector<int32_t> petIdsInHashOrder;

public:
	int32_t size() const;

	/**
	 * Returns an {@link PetTemplate} object with given id.
	 *
	 * @return PetTemplate object containing data about Pet with that id, nullptr (Java null) if there is none.
	 */
	const model::templates::pet::PetTemplate* getPetTemplate(int32_t id) const;

	const std::vector<int32_t>& getPetIds() const;
};

} // namespace aion::gameserver::dataholders
