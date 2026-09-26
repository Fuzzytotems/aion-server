#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/controllers/observer/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/zone/fwd.h"
#include "aion/gameserver/world/zone/fwd.h"
#include "aion/gameserver/world/zone/handler/GeneralZoneHandler.h"

namespace aion::gameserver::world::zone::handler {

/**
 * Zone handler of quest zones: attaches an observer (createObserver) to every player entering the zone and removes it when the player leaves.
 * <p>
 * Hub header (docs/design/hub-headers.md). Registered subclasses are created by the zone registry (HandlerRegistry.h
 * QuestZoneHandlerClass) with `static runtime::Ref<C> create(int32_t questId)`. The constructor takes the questId of the AION_ZONE_HANDLER
 * marker instead of reading the ZoneNameAnnotation of its own class, and keeps Java's check (IncompleteAnnotationException when the id is 0
 * or unknown to QUEST_DATA).
 *
 * @author Rolandas
 */
class QuestZoneHandler : public GeneralZoneHandler {
	AION_MAKE_REF_FRIEND
protected:
	runtime::HashMap<int32_t, runtime::Ref<controllers::observer::AbstractQuestZoneObserver>> observed{AION_LOCK_CLASS(QuestZoneHandler::observed)};
	const int32_t questId;

	/** Java: QuestZoneHandler() reading @ZoneNameAnnotation(questId) of getClass(); C++: the marker's questId (HandlerRegistry.h). */
	explicit QuestZoneHandler(int32_t questId);
	~QuestZoneHandler() override;

public:
	void onEnterZone(model::gameobjects::Creature& creature, ZoneInstance& zone) override;

	void onLeaveZone(model::gameobjects::Creature& creature, ZoneInstance& zone) override;

	/** Java: a new (anonymous) AbstractQuestZoneObserver for the player entering the zone */
	virtual runtime::Ref<controllers::observer::AbstractQuestZoneObserver> createObserver(model::gameobjects::player::Player& player,
		const model::templates::zone::ZoneTemplate* zoneTemplate) = 0;
};

} // namespace aion::gameserver::world::zone::handler
