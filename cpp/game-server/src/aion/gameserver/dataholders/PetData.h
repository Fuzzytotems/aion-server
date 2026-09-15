#pragma once

#include <cstdint>

#include "aion/gameserver/dataholders/PetData.xml.h"

namespace aion::gameserver::dataholders {

/** Java com.aionemu.gameserver.dataholders.PetData. @author IlBuono */
class PetData : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/PetData.xml.inc"
public:
	/**
	 * Returns an {@link PetTemplate} object with given id.
	 *
	 * @return PetTemplate object containing data about Pet with that id, nullptr (Java null) if there is none.
	 */
	const model::templates::pet::PetTemplate* getPetTemplate(int32_t id) const;
};

} // namespace aion::gameserver::dataholders
