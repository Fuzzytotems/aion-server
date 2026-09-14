#pragma once

#include "aion/gameserver/model/templates/pet/PetTemplate.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetTemplate. @author IlBuono */
class PetTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/pet/PetTemplate.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::pet
