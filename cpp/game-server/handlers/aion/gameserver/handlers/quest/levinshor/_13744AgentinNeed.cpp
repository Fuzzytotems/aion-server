#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::quest::levinshor {

// Hand-ported from game-server/data/handlers/quest/levinshor/_13744AgentinNeed.java (questgen refuses it: SiegeService.getInstance), in the questgen output's
// style: drafted by transliterating the copy with a placeholder for `SiegeService.getInstance().isSiegeInProgress(8011)`, then the call by hand. No correction of the Java code (lane C, 2026-10-18).

/**
 * @author Pad
 */
class _13744AgentinNeed final : public AbstractQuestHandler {
public:
	_13744AgentinNeed() : AbstractQuestHandler(13744) {}

	void register_() override {
		qe.registerQuestNpc(832840)->addOnTalkEvent(questId);
		qe.registerOnEnterZone(ZoneName::get("DRAGON_LORDS_SHRINE_600100000"), questId);
		qe.registerOnEnterZone(ZoneName::get("FLAMEBERTH_DOWNS_600100000"), questId);
		qe.registerOnKillInZone("DRAGON_LORDS_SHRINE_600100000", questId);
		qe.registerOnKillInZone("FLAMEBERTH_DOWNS_600100000", questId);
	}

	bool onEnterZoneEvent(QuestEnv& env, const ZoneName* zoneName) override {
		if ((zoneName == ZoneName::get("DRAGON_LORDS_SHRINE_600100000") || zoneName == ZoneName::get("FLAMEBERTH_DOWNS_600100000")) && ::aion::gameserver::services::SiegeService::getInstance().isSiegeInProgress(8011)) {
			runtime::Ptr<Player> player = env.getPlayer();
			runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
			if (qs == nullptr || qs->isStartable()) {
				return QuestService::startQuest(env);
			}
		}
		return false;
	}

	bool onKillInZoneEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr || qs->getStatus() != QuestStatus::START)
			return false;

		runtime::Ptr<VisibleObject> target = env.getVisibleObject();
		if (runtime::as<Player>(target) != nullptr && (player->isInsideZone(ZoneName::get("DRAGON_LORDS_SHRINE_600100000")) || player->isInsideZone(ZoneName::get("FLAMEBERTH_DOWNS_600100000"))) && ::aion::gameserver::services::SiegeService::getInstance().isSiegeInProgress(8011)) {
			if ((player->getLevel() >= ((runtime::cast<Player>(target))->getLevel() - 5)) && (player->getLevel() <= ((runtime::cast<Player>(target))->getLevel() + 9))) {
				int32_t var1 = qs->getQuestVarById(1);
				if (var1 >= 0 && var1 < 11) {
					qs->setQuestVarById(1, var1 + 1);
					updateQuestStatus(env);
					return true;
				} else if (var1 == 11) {
					qs->setQuestVarById(0, 1);
					qs->setQuestVarById(1, 0);
					qs->setStatus(QuestStatus::REWARD);
					updateQuestStatus(env);
					return true;
				}
			}
		}
		return false;
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs != nullptr && qs->getStatus() == QuestStatus::REWARD) {
			if (env.getTargetId() == 832840) {
				switch (env.getDialogActionId()) {
					case USE_OBJECT:
						return sendQuestDialog(env, 10002);
					case SELECT_QUEST_REWARD:
						return sendQuestDialog(env, 5);
					default:
						return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}
};
AION_QUEST_HANDLER(_13744AgentinNeed, 13744);

} // namespace aion::gameserver::handlers::quest::levinshor
