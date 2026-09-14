#pragma once

#include "aion/gameserver/model/templates/pet/PetRewards.xml.h"

namespace aion::gameserver::model::templates::pet {

/** Java com.aionemu.gameserver.model.templates.pet.PetRewards. @author Rolandas */
class PetRewards : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/pet/PetRewards.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::pet
