#include "aion/gameserver/model/templates/pet/PetTemplate.h"

#include "aion/gameserver/model/templates/pet/PetFunctionTypeInfo.h"

namespace aion::gameserver::model::templates::pet {

namespace {
/** Java PetFunction.CreateEmpty() appended by getPetFunctions (one shared immutable object instead of one per template) */
const PetFunction& emptyFunction() {
	static const PetFunction EMPTY = PetFunction::CreateEmpty();
	return EMPTY;
}
} // namespace

std::vector<const PetFunction*> PetTemplate::getPetFunctions() const {
	// Deviation: Java appends the empty function to the template's own list on the first call (unsynchronized lazy mutation of shared static
	// data, a race); the result is computed on each call and the template stays immutable (docs/deviations/P4-07a.md)
	std::vector<const PetFunction*> result;
	result.reserve(petFunctions.size() + 1);
	bool hasPlayerFuncs = false;
	for (const PetFunction& func : petFunctions) {
		result.push_back(&func);
		// Java: func.getPetFunctionType().isPlayerFunction() (an absent type throws NullPointerException there; the data has none)
		if (func.getPetFunctionType().has_value() && pet::isPlayerFunction(*func.getPetFunctionType()))
			hasPlayerFuncs = true;
	}
	if (!hasPlayerFuncs) // Java: petFunctions == null (absent == empty) or no player function
		result.push_back(&emptyFunction());
	return result;
}

const PetFunction* PetTemplate::getWarehouseFunction() const {
	// Java: null if petFunctions == null; after getPetFunctions() the list may also hold the empty NONE function, which is never a warehouse
	for (const PetFunction& pf : petFunctions) {
		if (pf.getPetFunctionType() == PetFunctionType::WAREHOUSE)
			return &pf;
	}
	return nullptr;
}

bool PetTemplate::containsFunction(PetFunctionType type) const {
	if (pet::getId(type) < 0)
		return false;
	for (const PetFunction* t : getPetFunctions()) {
		if (t->getPetFunctionType() == type)
			return true;
	}
	return false;
}

const PetFunction* PetTemplate::getPetFunction(PetFunctionType type) const {
	for (const PetFunction* t : getPetFunctions()) {
		if (t->getPetFunctionType() == type)
			return t;
	}
	return nullptr;
}

} // namespace aion::gameserver::model::templates::pet
