#pragma once

#include <vector>

#include "aion/gameserver/model/templates/instance_bonusatrr/InstanceBonusAttr.xml.h"

namespace aion::gameserver::model::templates::instance_bonusatrr {

/** Java com.aionemu.gameserver.model.templates.instance_bonusatrr.InstanceBonusAttr. @author xTz */
class InstanceBonusAttr : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/instance_bonusatrr/InstanceBonusAttr.xml.inc"
public:
	/** Java creates the list on first use (a write to the template); the C++ list always exists */
	const std::vector<InstancePenaltyAttr>& getPenaltyAttr() const { return penaltyAttr; }
};

} // namespace aion::gameserver::model::templates::instance_bonusatrr
