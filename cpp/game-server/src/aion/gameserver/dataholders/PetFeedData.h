#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/dataholders/PetFeedData.xml.h"

namespace aion::gameserver::dataholders {

/**
 * Java com.aionemu.gameserver.dataholders.PetFeedData.
 * <p>
 * C++: the index points into the bound `flavours` storage, which stays (Java clears the list). getPetFlavours returns the flavours in Java's
 * HashMap<Integer, PetFlavour> iteration order.
 *
 * @author Rolandas
 */
class PetFeedData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetFeedData.xml.inc"
private:
	std::unordered_map<int32_t, const model::templates::pet::PetFlavour*> petFlavoursById;
	/** C++ only: petFlavoursById.values() in Java's HashMap iteration order */
	std::vector<const model::templates::pet::PetFlavour*> flavoursInHashOrder;

public:
	/** @return the flavour, nullptr (Java null) if there is none */
	const model::templates::pet::PetFlavour* getFlavourById(int32_t flavourId) const;

	int32_t size() const;

	std::vector<const model::templates::pet::PetFlavour*> getPetFlavours() const;
};

} // namespace aion::gameserver::dataholders
