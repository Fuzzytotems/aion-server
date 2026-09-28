#include "aion/gameserver/model/templates/pet/PetFunction.h"

namespace aion::gameserver::model::templates::pet {

PetFunction PetFunction::CreateEmpty() {
	PetFunction result;
	result.type = PetFunctionType::NONE;
	return result;
}

} // namespace aion::gameserver::model::templates::pet
