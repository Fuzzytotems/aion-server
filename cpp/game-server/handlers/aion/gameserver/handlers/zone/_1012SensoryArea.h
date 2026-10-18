#pragma once

#include "aion/gameserver/handlers/zone/ZonePrelude.h"

namespace aion::gameserver::handlers::zone {

struct _1012SensoryArea_AbstractQuestZoneObserver;

/**
 * The zone handler of quest 1012's three sensory areas in Verteron ("LF1A_SENSORYAREA_Q1012_*"): a player who moves in one at step 1 goes
 * to step 2.
 * <p>
 * Java: data/handlers/zone/_1012SensoryArea.java, @ZoneNameAnnotation(value = "...", questId = 1012) (the marker is in the .cpp).
 *
 * @author Rolandas
 */
class _1012SensoryArea : public QuestZoneHandler {
	AION_MAKE_REF_FRIEND
	friend struct _1012SensoryArea_AbstractQuestZoneObserver;

protected:
	explicit _1012SensoryArea(int32_t questIdValue) : QuestZoneHandler(questIdValue) {}

public:
	/** The registry's factory (HandlerRegistry.h QuestZoneHandlerClass): Java's newInstance() with the marker's questId */
	static runtime::Ref<_1012SensoryArea> create(int32_t questId);

	runtime::Ref<AbstractQuestZoneObserver> createObserver(Player& player, const ZoneTemplate* zoneTemplate) override;
};

} // namespace aion::gameserver::handlers::zone
