#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/housing/AbstractHouseObject.xml.h"

namespace aion::gameserver::model::templates::housing {

/** Java com.aionemu.gameserver.model.templates.housing.AbstractHouseObject. @author Rolandas */
class AbstractHouseObject : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/templates/housing/AbstractHouseObject.xml.inc"
public:
	int32_t getTemplateId() const override { return id; }

	int32_t getL10nId() const override { return nameId; }

	/** Java returns null: the empty string */
	std::string getName() const override { return {}; }
};

} // namespace aion::gameserver::model::templates::housing
