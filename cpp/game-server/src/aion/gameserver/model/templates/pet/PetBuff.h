#pragma once

#include <vector>

#include "aion/gameserver/model/templates/pet/PetBuff.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetBuff. @author Rolandas */
class PetBuff : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetBuff.xml.inc"
public:
	/** Gets the value of the modifiers property (Java creates the list on first use; the C++ vector always exists). */
	const std::vector<::aion::gameserver::model::templates::stats::ModifiersTemplate>& getModifiers() const { return modifiers; }
};

} // namespace aion::gameserver::model::templates::pet
