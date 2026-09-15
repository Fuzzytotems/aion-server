#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/templates/quest/QuestDrop.xml.h"

namespace aion::gameserver::model::templates::quest {

/**
 * Java com.aionemu.gameserver.model.templates.quest.QuestDrop.
 * <p>
 * C++: the @XmlTransient `questId` is a C++-only member (xmlgen generates bound members only). Java's QuestEngine.init sets it on the published
 * template; templates are const after publication, so the holder sets it while loading (QuestsData, P4-09) and QuestEngine.init only reads it
 * (docs/deviations/P4-07b.md).
 *
 * @author MrPoke
 */
class QuestDrop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/QuestDrop.xml.inc"
	friend class HandlerSideDrop; // Java: same-package access to the protected fields of other drops

protected:
	/** Java @XmlTransient Integer questId */
	std::optional<int32_t> questId;

public:
	/** @return 100 without a chance attribute */
	int32_t getChance() const { return chance.value_or(100); }

	bool isDropEachMemberGroup() const { return dropEachMember == 1; }

	bool isDropEachMemberAlliance() const { return dropEachMember == 1 || dropEachMember == 2; }

	/** @return the questId */
	std::optional<int32_t> getQuestId() const { return questId; }

	/** @param questId the questId to set (before publication, see the class comment) */
	void setQuestId(std::optional<int32_t> questId);
};

} // namespace aion::gameserver::model::templates::quest
