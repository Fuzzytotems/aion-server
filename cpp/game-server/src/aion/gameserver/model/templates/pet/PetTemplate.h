#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aion/gameserver/model/templates/pet/PetTemplate.xml.h"
#include "aion/gameserver/model/templates/pet/PetFunctionType.h"

namespace aion::gameserver::model::templates::pet {

/**
 * Java com.aionemu.gameserver.model.templates.pet.PetTemplate.
 * <p>
 * C++: Java's getPetFunctions() appends an empty (NONE) function to the template's list on the first call when no function is a player function
 * (lazily, unsynchronized, on shared static data). The C++ template stays immutable: getPetFunctions() returns the bound functions plus the
 * shared empty function computed on each call (docs/deviations/P4-07a.md).
 *
 * @author IlBuono
 */
class PetTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/pet/PetTemplate.xml.inc"
public:
	int32_t getTemplateId() const override { return id; }

	std::string getName() const override { return name; }

	int32_t getL10nId() const override { return nameId; }

	/** @return the pet functions, followed by an empty (NONE) function if none of them is a player function (Java: never null) */
	std::vector<const PetFunction*> getPetFunctions() const;

	/** @return the warehouse function, nullptr (Java null) if there is none */
	const PetFunction* getWarehouseFunction() const;

	/** Used to write to SM_PET packet, so checks only needed ones */
	bool containsFunction(PetFunctionType type) const;

	/** Returns function if found, otherwise null */
	const PetFunction* getPetFunction(PetFunctionType type) const;
};

} // namespace aion::gameserver::model::templates::pet
