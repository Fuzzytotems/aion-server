#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.xml.h"

namespace aion::gameserver::model::templates::staticdoor {

/** Java com.aionemu.gameserver.model.templates.staticdoor.StaticDoorTemplate. @author Wakizashi */
class StaticDoorTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/staticdoor/StaticDoorTemplate.xml.inc"
public:
	int32_t getTemplateId() const override { return 300001; }

	std::string getName() const override { return "Door"; }

	int32_t getL10nId() const override { return 0; }
};

} // namespace aion::gameserver::model::templates::staticdoor
