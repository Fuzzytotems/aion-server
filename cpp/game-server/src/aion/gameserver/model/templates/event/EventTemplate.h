#pragma once

#include <cstdint>
#include <vector>

#include "aion/commons/configuration/Properties.h"
#include "aion/gameserver/model/templates/event/EventTemplate.xml.h"

namespace aion::gameserver::model::templates::event {

/** Java com.aionemu.gameserver.model.templates.event.EventTemplate. @author Rolandas, Neon */
class EventTemplate : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/event/EventTemplate.xml.inc"
public:
	bool hasConfigProperties() const { return configProperties.has_value(); }

	/**
	 * Java: Properties.load(new StringReader(String.join("\n", configProperties))).
	 *
	 * @throws NullPointerException (Java) without config_properties
	 */
	commons::configuration::Properties loadConfigProperties() const;

	/** Java returns Collections.emptyList() without a quests element */
	const std::vector<int32_t>& getStartableQuests() const;

	/** Java returns Collections.emptyList() without a quests element */
	const std::vector<int32_t>& getMaintainableQuests() const;

	bool isInEventPeriod(xml::adapters::LocalDateTime time) const;
};

} // namespace aion::gameserver::model::templates::event
