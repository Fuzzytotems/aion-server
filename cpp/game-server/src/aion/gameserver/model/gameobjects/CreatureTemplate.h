#pragma once

#include <optional>
#include <string>

#include "aion/gameserver/model/gameobjects/CreatureTemplate.xml.h"

namespace aion::gameserver::model::gameobjects {

/** Java com.aionemu.gameserver.model.gameobjects.CreatureTemplate. @author Neon */
class CreatureTemplate : public ::aion::gameserver::model::templates::VisibleObjectTemplate {
#include "aion/gameserver/model/gameobjects/CreatureTemplate.xml.inc"
public:
	/** @return the AI name, nullopt (Java null) unless a subclass has one */
	virtual std::optional<std::string> getAiName() const { return std::nullopt; }
};

} // namespace aion::gameserver::model::gameobjects
