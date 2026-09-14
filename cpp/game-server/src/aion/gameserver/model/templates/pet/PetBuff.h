#pragma once

#include "aion/gameserver/model/templates/pet/PetBuff.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetBuff. @author Rolandas */
class PetBuff : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetBuff.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::pet
