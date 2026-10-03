#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <array>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ITEM_USAGE_ANIMATION.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::handlers::quest::heiron {

// Hand-ported from game-server/data/handlers/quest/heiron/_3200PriceOfGoodwill.java (questgen refuses it: the anonymous Runnables of :102 and
// :134; WorldMapInstance.getNpc of :85 is outside its API table), statement by statement in the questgen output's style
// (docs/deviations/Q03.md, Hand ports).

/**
 * @author kecimis
 */
class _3200PriceOfGoodwill final : public AbstractQuestHandler {
private:
	// fieldmap: a table of literals that is never written (Java's effectively final int[]), questgen's static constexpr std::array
	static constexpr std::array<int32_t, 5> npc_ids{204658, 798332, 700522, 279006, 798322};

	/*
	 * 204658 - Roikinerk 798332 - Haorunerk 700522 - Haorunerks Bag 279006 - Garkbinerk 798322 - Kuruminerk
	 */

public:
	_3200PriceOfGoodwill() : AbstractQuestHandler(3200) {}

	void register_() override {
		qe.registerQuestNpc(204658)->addOnQuestStart(questId); // Roikinerk
		qe.registerQuestItem(182209082, questId); // Teleport Scroll
		for (int32_t npc_id : npc_ids)
			qe.registerQuestNpc(npc_id)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs == nullptr || qs->isStartable()) {
			if (targetId == 204658) { // Roikinerk
				if (env.getDialogActionId() == QUEST_SELECT)
					return sendQuestDialog(env, 4762);
				else
					return sendQuestStartDialog(env);
			}
			return false;
		}

		int32_t var = qs->getQuestVarById(0);

		if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 798322) { // Kuruminerk
				if (env.getDialogActionId() == USE_OBJECT)
					return sendQuestDialog(env, 10002);
				else if (env.getDialogActionId() == SELECT_QUEST_REWARD)
					return sendQuestDialog(env, 5);
				else
					return sendQuestEndDialog(env);
			}
			return false;
		} else if (qs->getStatus() == QuestStatus::START) {
			if (targetId == 204658 && var == 0) { // Roikinerk
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 1003);
					case SETPRO1: {
						runtime::Ptr<WorldMapInstance> steelRake = InstanceService::getNextAvailableInstance(::aion::gameserver::world::getId(WorldMapType::STEEL_RAKE), *player);
						runtime::Ptr<Npc> haorunerksCorpse = steelRake->getNpc(798333);
						spawn(798332, *steelRake, haorunerksCorpse->getX(), haorunerksCorpse->getY(), haorunerksCorpse->getZ(), static_cast<int8_t>(30));
						haorunerksCorpse->getController().delete_();
						TeleportService::teleportTo(*player, *steelRake, 403.55f, 508.11f, 885.77f);
						return defaultCloseDialog(env, 0, 1);
					}
				}
			} else if (targetId == 798332 && var == 1) { // Haorunerk
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 1352);
					case SELECT2_1:
						playQuestMovie(env, 431);
						break;
					case SETPRO2:
						return defaultCloseDialog(env, 1, 2);
				}
			} else if (targetId == 700522 && var == 2) { // Haorunerks Bag, loc: 401.24 503.19 885.76 119
				// anonymous Runnable at _3200PriceOfGoodwill.java:102 (fieldmap _3200PriceOfGoodwill$1, storage: task): its captures - this
				// handler (Immortal, RT-11) and the env - are the Pin of the task
				ThreadPoolManager::getInstance().schedule({this, &env}, [this, &env] { updateQuestStatus(env); }, 3000);
				return true;
			} else if (targetId == 279006 && var == 3) { // Garkbinerk
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 2034);
					case SET_SUCCEED:
						return defaultCloseDialog(env, 3, 3, true, false);
				}
			}
		}
		return false;
	}

	HandlerResult onItemUseEvent(QuestEnv& env, Item& item) override {
		runtime::Ptr<Player> player = env.getPlayer();
		int32_t id = item.getItemTemplate()->getTemplateId();
		int32_t itemObjId = item.getObjectId();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		if (id != 182209082 || qs == nullptr || qs->getQuestVarById(0) != 2)
			return HandlerResult::UNKNOWN;

		PacketSendUtility::broadcastPacket(*player, SM_ITEM_USAGE_ANIMATION(player->getObjectId(), itemObjId, id, 3000, 0, 0), true);
		// anonymous Runnable at _3200PriceOfGoodwill.java:134 (fieldmap _3200PriceOfGoodwill$2, storage: task): its captures - this handler
		// (Immortal, RT-11), the player, the env and the quest state (never null here: the guard above returned) - are the Pin of the task; the
		// two ints are copied
		Player& user = *player;
		QuestState& state = *qs;
		ThreadPoolManager::getInstance().schedule({this, &user, &env, &state},
			[this, &user, &env, &state, itemObjId, id] {
				PacketSendUtility::broadcastPacket(user, SM_ITEM_USAGE_ANIMATION(user.getObjectId(), itemObjId, id, 0, 1, 0), true);
				removeQuestItem(env, 182209082, 1);
				// teleport location(BlackCloudIsland): 400010000 3419.16 2445.43 2766.54 57
				TeleportService::teleportTo(user, 400010000, 3419.16f, 2445.43f, 2766.54f, static_cast<int8_t>(57));
				state.setQuestVarById(0, state.getQuestVarById(0) + 1);
				updateQuestStatus(env);
			},
			3000);
		return HandlerResult::SUCCESS;
	}
};
AION_QUEST_HANDLER(_3200PriceOfGoodwill, 3200);

} // namespace aion::gameserver::handlers::quest::heiron
