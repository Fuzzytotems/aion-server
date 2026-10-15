#include "aion/gameserver/model/templates/event/EventTemplate.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::templates::event {

commons::configuration::Properties EventTemplate::loadConfigProperties() const {
	if (!configProperties)
		throw runtime::NullPointerException("EventTemplate " + name + " has no config properties");
	std::string text;
	for (size_t i = 0; i < configProperties->size(); ++i) {
		if (i > 0)
			text += '\n';
		text += (*configProperties)[i];
	}
	commons::configuration::Properties properties;
	properties.loadUtf8(text);
	return properties;
}

const std::vector<int32_t>& EventTemplate::getStartableQuests() const {
	static const std::vector<int32_t> empty;
	return quests == nullptr ? empty : quests->getStartableQuests();
}

const std::vector<int32_t>& EventTemplate::getMaintainableQuests() const {
	static const std::vector<int32_t> empty;
	return quests == nullptr ? empty : quests->getMaintainQuests();
}

bool EventTemplate::isInEventPeriod(xml::adapters::LocalDateTime time) const {
	return (!startDate || !(time < *startDate)) && (!endDate || time < *endDate);
}

} // namespace aion::gameserver::model::templates::event
