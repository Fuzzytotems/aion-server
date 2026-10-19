#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <cmath>
#include "aion/commons/utils/Rnd.h"

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/handlers/HandlerResultInfo.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"
#include "aion/gameserver/world/zone/ZoneName.h"

namespace aion::gameserver::handlers::quest::tiamat_stronghold {

// Hand-ported from game-server/data/handlers/quest/tiamat_stronghold/_30721OminousDebrisEnergy.java (questgen refuses it: Rnd.nextFloat, Math.toRadians/cos/sin, Vector3f and the SpawnEngine calls of rndSpawnInRange), in the questgen output's
// style: drafted by transliterating the copy without rndSpawnInRange's body and with 4f for `Rnd.nextFloat(4f)`, then both by hand (DrakanPriestAI's spawn on a circle). No correction of the Java code (lane C, 2026-10-18).

/**
 * @author Estrayl
 */
class _30721OminousDebrisEnergy final : public AbstractQuestHandler {
private:
	static constexpr int32_t QUEST_ITEM_ID = 182215698; // Sedative
	static constexpr int32_t START_NPC_ID = 804704; // Eukraton
	static constexpr int32_t TALK_NPC_1_ID = 804870; // Monroe
	static constexpr int32_t TALK_NPC_2_ID = 804868; // Rosalee
	static constexpr int32_t ATTACKER_NPC_ID = 217424; // Mirror Image

public:
	_30721OminousDebrisEnergy() : AbstractQuestHandler(30721) {}

	void register_() override {
		qe.registerQuestNpc(START_NPC_ID)->addOnQuestStart(questId);
		qe.registerQuestNpc(TALK_NPC_1_ID)->addOnTalkEvent(questId);
		qe.registerQuestNpc(TALK_NPC_2_ID)->addOnTalkEvent(questId);
		qe.registerQuestItem(QUEST_ITEM_ID, questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<QuestState> qs = env.getPlayer()->getQuestStateList()->getQuestState(questId);
		int32_t dialogActionId = env.getDialogActionId();
		int32_t targetId = env.getTargetId();

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == START_NPC_ID) {
				if (dialogActionId == QUEST_SELECT) {
					return sendQuestDialog(env, 4762);
				} else {
					return sendQuestStartDialog(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (targetId == TALK_NPC_1_ID && var == 0) {
				if (dialogActionId == QUEST_SELECT) {
					return sendQuestDialog(env, 1011);
				} else if (dialogActionId == SETPRO1) {
					changeQuestStep(env, 0, 1);
					return closeDialogWindow(env);
				}
			} else if (targetId == TALK_NPC_2_ID) {
				if (dialogActionId == QUEST_SELECT) {
					if (var == 1) {
						return sendQuestDialog(env, 1352);
					} else if (var == 3) {
						return sendQuestDialog(env, 2034);
					}
				} else if (dialogActionId == SETPRO2) {
					changeQuestStep(env, 1, 2);
					giveQuestItem(env, QUEST_ITEM_ID, 1);
					return closeDialogWindow(env);
				} else if (dialogActionId == SET_SUCCEED) {
					changeQuestStep(env, 3, 4, true);
					return closeDialogWindow(env);
				}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == TALK_NPC_1_ID) {
				if (dialogActionId == QUEST_SELECT) {
					return sendQuestDialog(env, 10002);
				} else {
					return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			if (player->isInsideItemUseZone(ZoneName::get("LF5_ITEMUSEAREA_Q30721"))) {
				int32_t var = qs->getQuestVarById(0);
				if (var == 2) {
					bool isItemUseComplete = useQuestItem(env, item, 2, 3, false);
					if (isItemUseComplete) {
						ThreadPoolManager::getInstance().schedule({this}, [this, player = runtime::Ref<Player>(player)] {
							rndSpawnInRange(player, ::aion::commons::utils::Rnd::nextFloat(4.0f));
							rndSpawnInRange(player, ::aion::commons::utils::Rnd::nextFloat(4.0f));
						}, 1500);
					}
					return ::aion::gameserver::questEngine::handlers::fromBoolean(isItemUseComplete);
				}
			}
		}
		return HandlerResult::FAILED;
	}

private:
	// Java rndSpawnInRange (Math.toRadians: times DEGREES_TO_RADIANS; cos and sin in double, then the float casts, as DrakanPriestAI)
	void rndSpawnInRange(runtime::Ptr<Player> player, float distance) {
		world::WorldPosition& p = *player->getPosition();
		double angleRadians = static_cast<double>(::aion::commons::utils::Rnd::nextFloat(360.0f)) * 0.017453292519943295; // Math.toRadians
		float x = p.getX() + static_cast<float>(std::cos(angleRadians) * distance);
		float y = p.getY() + static_cast<float>(std::sin(angleRadians) * distance);
		geoEngine::math::Vector3f pos = world::geo::GeoService::getInstance().getClosestCollision(*player, x, y, p.getZ());
		runtime::Ref<SpawnTemplate> spawnTemplate = spawnengine::SpawnEngine::newSingleTimeSpawn(p.getMapId(), ATTACKER_NPC_ID, pos.getX(), pos.getY(),
			pos.getZ(), int8_t{0});
		runtime::Ptr<Npc> npc = runtime::cast<Npc>(spawnengine::SpawnEngine::spawnObject(*spawnTemplate, p.getInstanceId()));
		npc->getAggroList().addHate(*player, 1000);
	}
};
AION_QUEST_HANDLER(_30721OminousDebrisEnergy, 30721);

} // namespace aion::gameserver::handlers::quest::tiamat_stronghold
