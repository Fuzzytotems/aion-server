#pragma once

#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/quest/XMLStartCondition.xml.h"

namespace aion::gameserver::model::templates::quest {

/** Java com.aionemu.gameserver.model.templates.quest.XMLStartCondition. @author antness, vlog */
class XMLStartCondition : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/quest/XMLStartCondition.xml.inc"
public:
	bool isOptional() const { return !finished.empty(); }

private:
	/** Check, if the player has finished listed quests */
	bool checkFinishedQuests(gameobjects::player::QuestStateList& qsl) const;

	/** Check, if the player has not finished listed quests */
	bool checkUnfinishedQuests(gameobjects::player::QuestStateList& qsl) const;

	/** Check, if the player has not acquired listed quests */
	bool checkNoAcquiredQuests(gameobjects::player::QuestStateList& qsl) const;

	/** Check, if the player has acquired listed quests */
	bool checkAcquiredQuests(gameobjects::player::QuestStateList& qsl) const;

	bool checkEquippedItems(gameobjects::player::Player& player, bool warn) const;

	bool isRequiredTitleDisplayed(gameobjects::player::Player& player) const;

public:
	/** Check all conditions */
	bool check(gameobjects::player::Player& player, bool warn) const;
};

} // namespace aion::gameserver::model::templates::quest
