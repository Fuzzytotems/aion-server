#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/ActionItemUseOperation.h"

#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_USE_OBJECT.h"
#include "aion/gameserver/questEngine/handlers/models/xmlQuest/operations/QuestOperations.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/TaskConcepts.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations {

using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_EMOTION;
using network::aion::serverpackets::SM_USE_OBJECT;
using utils::PacketSendUtility;

void ActionItemUseOperation::doOperate(model::QuestEnv& env) const {
	runtime::Ptr<Player> player = env.getPlayer();
	runtime::Ptr<Npc> npc = runtime::as<Npc>(env.getVisibleObject());
	if (!npc)
		return;
	const int32_t defaultUseTime = 3000;
	PacketSendUtility::sendPacket(*player, SM_USE_OBJECT(player->getObjectId(), npc->getObjectId(), defaultUseTime, 1));
	PacketSendUtility::broadcastPacket(*player, SM_EMOTION(*player, gameserver::model::EmotionType::START_QUESTLOOT, 0, npc->getObjectId()), true);
	// anonymous Runnable at ActionItemUseOperation.java:38 (fieldmap ActionItemUseOperation$1): its captures - this template, the use time, the
	// npc, the player and the env - are the TaskArgs of an unpinned task; the Refs retain the three objects as the Java capture does
	utils::ThreadPoolManager::getInstance().schedule(
		runtime::bindTask(
			[](const ActionItemUseOperation* operation, int32_t useTime, Npc& target, Player& user, model::QuestEnv& taskEnv) {
				PacketSendUtility::sendPacket(user, SM_USE_OBJECT(user.getObjectId(), target.getObjectId(), useTime, 0));
				if (!operation->finish) // Java: NullPointerException on the null <finish> (a required element)
					throw runtime::NullPointerException("ActionItemUseOperation.finish");
				operation->finish->operate(taskEnv);
			},
			this, defaultUseTime, runtime::Ref<Npc>(npc), runtime::Ref<Player>(player), runtime::Ref<model::QuestEnv>(env)),
		defaultUseTime);
}

} // namespace aion::gameserver::questEngine::handlers::models::xmlQuest::operations
