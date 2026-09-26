#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/gather/GatherableTemplate.xml.h"

namespace aion::gameserver::model::templates::gather {

/** Java com.aionemu.gameserver.model.templates.gather.GatherableTemplate. @author ATracer, KID */
class GatherableTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/gather/GatherableTemplate.xml.inc"
public:
	int32_t getTemplateId() const override { return id; }

	std::string getName() const override { return name; }

	int32_t getL10nId() const override { return nameId; }
};

} // namespace aion::gameserver::model::templates::gather
