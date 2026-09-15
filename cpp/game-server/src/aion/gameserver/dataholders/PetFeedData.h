#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/PetFeedData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetFeedData. @author Rolandas */
class PetFeedData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetFeedData.xml.inc"
public:
	/** @return the flavour, nullptr (Java null) if there is none */
	const model::templates::pet::PetFlavour* getFlavourById(int32_t flavourId) const;
};

} // namespace aion::gameserver::dataholders
