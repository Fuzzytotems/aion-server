#pragma once

#include "aion/gameserver/model/templates/rewards/MedicineItem.xml.h"

namespace aion::gameserver::model::templates::rewards {

/** Java com.aionemu.gameserver.model.templates.rewards.MedicineItem. */
class MedicineItem : public ::aion::gameserver::model::templates::rewards::IdLevelReward {
#include "aion/gameserver/model/templates/rewards/MedicineItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::rewards
