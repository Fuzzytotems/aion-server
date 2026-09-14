#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/QuestTemplate.xml.h"
#include "aion/gameserver/model/templates/L10n.h"

namespace aion::gameserver::model::templates {

/** Java com.aionemu.gameserver.model.templates.QuestTemplate. @author MrPoke, vlog, Neon */
class QuestTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/model/templates/QuestTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }

	bool isTimeBased() const { return repeatCycle.has_value(); }
	/** Java returns Collections.emptyList() for a quest without drops; the C++ list always exists */
	const std::vector<quest::QuestDrop>& getQuestDrop() const { return questDrop; }
};

} // namespace aion::gameserver::model::templates
