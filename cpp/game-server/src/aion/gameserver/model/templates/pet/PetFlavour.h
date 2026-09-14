#pragma once

#include "aion/gameserver/model/templates/pet/PetFlavour.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetFlavour. @author Rolandas */
class PetFlavour : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetFlavour.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::pet
