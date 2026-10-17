#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>
#include <string_view>

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::handlers::quest::eltnen {

// Hand-ported from game-server/data/handlers/quest/eltnen/_1354PraticalAerobatics.java (questgen refuses it: its String[] field and
// Player.getLifeStats().increaseFp, which are outside questgen's vocabulary), in the questgen output's style (the flying rings as
// _1044TestingFlightSkills.cpp has them), with no correction of the Java code (lane C, 2026-10-14; docs/deviations/Q05.md, "1354").

/**
 * @author Atomics
 */
class _1354PraticalAerobatics final : public AbstractQuestHandler {
private:
	// chronological order of the flight rings has been changed (Java :21-23: a field never written)
	static constexpr std::array<std::string_view, 7> rings{"ERACUS_TEMPLE_210020000_1", "ERACUS_TEMPLE_210020000_4", "ERACUS_TEMPLE_210020000_3",
		"ERACUS_TEMPLE_210020000_6", "ERACUS_TEMPLE_210020000_5", "ERACUS_TEMPLE_210020000_2", "ERACUS_TEMPLE_210020000_7"};

public:
	_1354PraticalAerobatics() : AbstractQuestHandler(1354) {}

	void register_() override {
		qe.registerQuestNpc(203983)->addOnQuestStart(questId);
		qe.registerQuestNpc(203983)->addOnTalkEvent(questId);
		qe.registerOnQuestTimerEnd(questId);
		for (std::string_view ring : rings) {
			qe.registerOnPassFlyingRings(ring, questId);
		}
	}

	bool onPassFlyingRingEvent(QuestEnv& env, std::string_view flyingRing) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			if (rings[0] == flyingRing)
				changeQuestStep(env, 1, 2);
			else if (rings[1] == flyingRing)
				changeQuestStep(env, 2, 3);
			else if (rings[2] == flyingRing)
				changeQuestStep(env, 3, 4);
			else if (rings[3] == flyingRing)
				changeQuestStep(env, 4, 5);
			else if (rings[4] == flyingRing)
				changeQuestStep(env, 5, 6);
			else if (rings[5] == flyingRing)
				changeQuestStep(env, 6, 7);
			else if (rings[6] == flyingRing) {
				qs->setQuestVarById(0, 8);
				changeQuestStep(env, 8, 8, true);
				QuestService::questTimerEnd(env);
			}
			applyFlightRingEffect(env);
			return true;
		}
		return false;
	}

	bool onQuestTimerEndEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs == nullptr)
			return false;

		qs->setQuestVarById(0, 0);
		updateQuestStatus(env);
		return true;
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t targetId = 0;
		int32_t dialogActionId = env.getDialogActionId();
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == 203983) {
				if (dialogActionId == QUEST_SELECT)
					return sendQuestDialog(env, 1011);
				else
					return sendQuestStartDialog(env);
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 203983) {
				int32_t var0 = qs->getQuestVarById(0);
				switch (dialogActionId) {
					case QUEST_SELECT:
						if (var0 == 0)
							return sendQuestDialog(env, 1003);
						else if (var0 == 1)
							return sendQuestDialog(env, 2717);
						else if (var0 == 8)
							return sendQuestDialog(env, 2375);
						return false;
					case SETPRO1:
						if (qs->getQuestVarById(0) == 0) {
							QuestService::questTimerStart(env, 120);
							return defaultCloseDialog(env, 0, 1);
						}
						return false;
					case SELECT_QUEST_REWARD:
						return sendQuestEndDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203983)
				return sendQuestEndDialog(env);
		}
		return false;
	}

private:
	void applyFlightRingEffect(QuestEnv& env) {
		runtime::Ptr<Player> player = env.getPlayer();
		player->getLifeStats()->increaseFp(network::aion::serverpackets::SM_ATTACK_STATUS_TYPE::FP_RINGS, 7, 0,
			network::aion::serverpackets::SM_ATTACK_STATUS_LOG::REGULAR);
		SkillEngine::getInstance().applyEffectDirectly(1856, *player, *player);
	}
};
AION_QUEST_HANDLER(_1354PraticalAerobatics, 1354);

} // namespace aion::gameserver::handlers::quest::eltnen
