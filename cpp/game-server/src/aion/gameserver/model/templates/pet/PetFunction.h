#pragma once

#include "aion/gameserver/model/templates/pet/PetFunction.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetFunction. @author IlBuono */
class PetFunction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::pet
