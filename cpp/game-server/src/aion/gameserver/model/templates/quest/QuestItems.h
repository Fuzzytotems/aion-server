#pragma once

#include <cstdint>

#include "aion/gameserver/model/templates/quest/QuestItems.xml.h"

namespace aion::gameserver::model::templates::quest {

/**
 * Java com.aionemu.gameserver.model.templates.quest.QuestItems.
 * <p>
 * C++: a value type (quest rewards built at run time, e.g. by QuestService, are copies, not template pointers).
 *
 * @author MrPoke
 */
class QuestItems : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestItems.xml.inc"
public:
	/** Constructor used by unmarshaller (Java sets count = 1, which the member initializer does) */
	QuestItems() = default;

	QuestItems(int32_t itemId, int64_t count);
};

} // namespace aion::gameserver::model::templates::quest
