#include "aion/gameserver/handlers/quest/QuestPrelude.h"

#include <algorithm>

#include <array>

#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/services/QuestService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/controllers/movement/NpcMoveController.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/KnownObject.h"

namespace aion::gameserver::handlers::quest::eltnen {

// Hand-ported from game-server/data/handlers/quest/eltnen/_14026ALoneDefense.java (questgen refuses it: two lambdas,
// `Arrays.stream(mobs).anyMatch(...)` and the knownlist's `findObject(o -> o.get() instanceof Npc npc && ...)`, the int[] varargs,
// `Rnd.get(int[])` and the walk to Kargate's helper), in the questgen output's style: drafted by transliterating a copy of the Java with the
// varargs inlined, the anyMatch as a loop (here std::ranges::any_of, as rule stream-any-match writes it) and the spawned npc's local renamed (`spawn` shadows the helper), then the random mob
// and the walk written by hand. No correction of the Java code (lane C, 2026-10-15; docs/deviations/Q05.md, "14026").

/**
 * @author Artur
 */
class _14026ALoneDefense final : public AbstractQuestHandler {
private:
	static constexpr std::array<int32_t, 3> mobs{211628, 211630, 213575};

public:
	_14026ALoneDefense() : AbstractQuestHandler(14026) {}

	void register_() override {
		qe.registerOnQuestCompleted(questId);
		qe.registerOnLevelChanged(questId);
		qe.registerOnDie(questId);
		qe.registerOnLogOut(questId);
		qe.registerOnQuestTimerEnd(questId);
		qe.registerQuestNpc(203901)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204020)->addOnTalkEvent(questId);
		qe.registerQuestNpc(204044)->addOnTalkEvent(questId);
		for (int32_t mob : mobs) {
			qe.registerQuestNpc(mob)->addOnKillEvent(questId);
		}
		qe.registerQuestNpc(700141)->addOnTalkEvent(questId);
	}

	bool onDialogEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);

		int32_t targetId = 0;
		if (runtime::as<Npc>(env.getVisibleObject()) != nullptr)
			targetId = (runtime::cast<Npc>(env.getVisibleObject()))->getNpcId();

		if (qs == nullptr)
			return false;

		if (qs->getStatus() == QuestStatus::START) {
			switch (targetId) {
				case 203901:
					switch (env.getDialogActionId()) {
						case QUEST_SELECT:
							{
								if (qs->getQuestVarById(0) == 0) {
									return sendQuestDialog(env, 1011);
								}
								return false;
							}
						case SETPRO1:
							{
								qs->setQuestVar(1);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								TeleportService::teleportTo(*player, ::aion::gameserver::world::getId(WorldMapType::ELTNEN), 1596.1948f, 1529.9152f, 317, static_cast<int8_t>(120), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					return false;
				case 204020:
					switch (env.getDialogActionId()) {
						case QUEST_SELECT:
							{
								if (qs->getQuestVarById(0) == 1) {
									return sendQuestDialog(env, 1352);
								}
								return false;
							}
						case SETPRO2:
							{
								qs->setQuestVar(2);
								updateQuestStatus(env);
								giveQuestItem(env, 182215324, 1);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								TeleportService::teleportTo(*player, ::aion::gameserver::world::getId(WorldMapType::ELTNEN), 2500.15f, 780.9f, 409, static_cast<int8_t>(15), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
					return false;
				case 204044:
					switch (env.getDialogActionId()) {
						case QUEST_SELECT:
							{
								switch (qs->getQuestVarById(0)) {
									case 2:
										{
											return sendQuestDialog(env, 1693);
										}
									case 4:
										{
											return sendQuestDialog(env, 2034);
										}
								}
								return false;
							}
						case SETPRO3:
							{
								qs->setQuestVar(3);
								updateQuestStatus(env);
								PacketSendUtility::sendPacket(*player, SM_DIALOG_WINDOW(env.getVisibleObject()->getObjectId(), 10));
								QuestService::questTimerStart(env, 180);
								spawn(player);
								return true;
							}
						case SETPRO4:
							{
								qs->setStatus(QuestStatus::REWARD);
								updateQuestStatus(env);
								TeleportService::teleportTo(*player, ::aion::gameserver::world::getId(WorldMapType::ELTNEN), 271.69f, 2787.04f, 272.47f, static_cast<int8_t>(50), TeleportAnimation::FADE_OUT_BEAM);
								return true;
							}
					}
			}
		} else if (qs->getStatus() == QuestStatus::REWARD) {
			if (targetId == 203901) {
				switch (env.getDialogActionId()) {
					case QUEST_SELECT:
						return sendQuestDialog(env, 2375);
					default:
						qs->setRewardGroup(0); // group 0 and 1 are identical in templates, set anyway to mute warning
						return sendQuestEndDialog(env);
				}
			}
		}
		return false;
	}

	void onQuestCompletedEvent(QuestEnv& env) override {
		defaultOnQuestCompletedEvent(env, {14020, 14021, 14022, 14023, 14024, 14025});
	}

	void onLevelChangedEvent(Player& player) override {
		defaultOnLevelChangedEvent(player, {14020, 14021, 14022, 14023, 14024, 14025});
	}

	bool onQuestTimerEndEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (var == 3) {
				qs->setQuestVar(4);
				updateQuestStatus(env);
				return true;
			}
		}
		return false;
	}

	bool onDieEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (var == 3) {
				changeQuestStep(env, var, 2);
				return true;
			}
		}
		return false;
	}

	bool onLogOutEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (var == 3) {
				changeQuestStep(env, var, 2);
				return true;
			}
		}
		return false;
	}

	bool onKillEvent(QuestEnv& env) override {
		runtime::Ptr<Player> player = env.getPlayer();
		runtime::Ptr<QuestState> qs = player->getQuestStateList()->getQuestState(questId);
		if (qs != nullptr && qs->getStatus() == QuestStatus::START) {
			int32_t var = qs->getQuestVarById(0);
			if (var == 3) {
				int32_t targetId = env.getTargetId();
				if (std::ranges::any_of(mobs, [&](int32_t npcId) { return npcId == targetId; })) {
					spawn(player);
					return true;
				}
			}
		}
		return false;
	}

private:
	void spawn(runtime::Ptr<Player> player) {
		int32_t mobToSpawn = ::aion::commons::utils::Rnd::get(mobs);
		float x = 0;
		float y = 0;
		float z = 217.48f;
		switch (mobToSpawn) {
			case 211628:
				x = 254.74f;
				y = 236.72f;
				break;
			case 211630:
				x = 257.92f;
				y = 237.39f;
				break;
			case 213575:
				x = 261.86f;
				y = 237.5f;
				break;
		}
		runtime::Ptr<Npc> spawned = runtime::cast<Npc>(AbstractQuestHandler::spawn(mobToSpawn, *player, x, y, z, static_cast<int8_t>(95)));
		runtime::Ptr<VisibleObject> target = spawned->getKnownList().findObject([](world::knownlist::KnownObject& o) {
			runtime::Ptr<Npc> npc = runtime::as<Npc>(o.get());
			return npc != nullptr && npc->getNpcId() == 204020;
		});
		if (target != nullptr) {
			spawned->setTarget(target);
			spawned->getAi().setStateIfNot(ai::AIState::WALKING);
			spawned->setState(CreatureState::ACTIVE, true);
			spawned->getMoveController()->moveToTargetObject();
			PacketSendUtility::broadcastPacket(*spawned, SM_EMOTION(*spawned, EmotionType::CHANGE_SPEED, 0, spawned->getObjectId()));
		}
	}
};
AION_QUEST_HANDLER(_14026ALoneDefense, 14026);

} // namespace aion::gameserver::handlers::quest::eltnen
