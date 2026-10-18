#include "aion/gameserver/handlers/zone/_1012SensoryArea.h"

#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestVars.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::handlers::zone {

AION_ZONE_HANDLER(_1012SensoryArea, "LF1A_SENSORYAREA_Q1012_2_206005_4_210030000 LF1A_SENSORYAREA_Q1012_3_206006_6_210030000 LF1A_SENSORYAREA_Q1012_1_206004_8_210030000", 1012);

/**
 * Java: the anonymous AbstractQuestZoneObserver of createObserver (_1012SensoryArea.java:23-36, fieldmap zone._1012SensoryArea$1). It
 * captures the handler (Java's enclosing instance, for questId); Java's `player` is the observer's own inherited field, the same player.
 */
struct _1012SensoryArea_AbstractQuestZoneObserver final : AbstractQuestZoneObserver {
	AION_MAKE_REF_FRIEND

	const runtime::Ref<_1012SensoryArea> sensoryArea; // captured this _1012SensoryArea this (line 34)

	_1012SensoryArea_AbstractQuestZoneObserver(_1012SensoryArea& handler, Player& player, const ZoneTemplate* zoneTemplate)
		: AbstractQuestZoneObserver(player, zoneTemplate), sensoryArea(handler) {}

	static runtime::Ref<_1012SensoryArea_AbstractQuestZoneObserver> create(_1012SensoryArea& handler, Player& player,
		const ZoneTemplate* zoneTemplate) {
		return runtime::makeRef<_1012SensoryArea_AbstractQuestZoneObserver>(handler, player, zoneTemplate);
	}

	// Java _1012SensoryArea.java:25-35
	void onMoved(float /*distanceScouted*/, float /*distanceToCenter*/, int32_t /*steps*/, int64_t /*timeSpent*/) override {
		// Another way to do the same is like this:
		//
		// QuestEngine.getInstance().onEnterZone(new QuestEnv(null, player, questId), observedZone.getName());
		//
		// In this case, it will be handled inside the quest script. But you need to register
		// each zone in the quest script. For scouting you may save distance, time and send the event back
		// to the QuestEngine when the criteria are met. It will work like a delayed onEnterZone event.
		const runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(sensoryArea->questId);
		if (qs == nullptr || qs->getQuestVars()->getQuestVars() != 1)
			return;
		qs->setQuestVarById(0, 2); // 2
		PacketSendUtility::sendPacket(*player, SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs));
	}
};

runtime::Ref<_1012SensoryArea> _1012SensoryArea::create(int32_t questId) {
	return runtime::makeRef<_1012SensoryArea>(questId);
}

// Java _1012SensoryArea.java:21-37
runtime::Ref<AbstractQuestZoneObserver> _1012SensoryArea::createObserver(Player& player, const ZoneTemplate* zoneTemplate) {
	return _1012SensoryArea_AbstractQuestZoneObserver::create(*this, player, zoneTemplate);
}

} // namespace aion::gameserver::handlers::zone
