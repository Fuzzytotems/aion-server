#include "aion/gameserver/model/instance/InstanceBuff.h"

#include <algorithm>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceBuffData.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/InstanceScoreType.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/instance_bonusatrr/InstanceBonusAttr.h"
#include "aion/gameserver/model/templates/instance_bonusatrr/InstancePenaltyAttr.h"
#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"
#include "aion/gameserver/network/aion/instanceinfo/HarmonyScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::model::instance {

// Java InstanceBuff.java:38-40
InstanceBuff::InstanceBuff(int32_t buffId) : instanceBonusAttr(dataholders::DataManager::INSTANCE_BUFF_DATA->getInstanceBonusattr(buffId)) {
}

InstanceBuff::~InstanceBuff() = default;

runtime::Ref<InstanceBuff> InstanceBuff::create(int32_t buffId) {
	return runtime::makeRef<InstanceBuff>(buffId);
}

// Java InstanceBuff.java:42-59
void InstanceBuff::applyEffect(gameobjects::player::Player& player, int32_t time) {

	if (isActive() || instanceBonusAttr == nullptr) {
		return;
	}
	if (time != 0) {
		// Java: new InstanceBuffTask(player) - the inner class's run() is endEffect(player); the task holds this buff and the player
		task.set(utils::ThreadPoolManager::getInstance().schedule({this}, [this, owner = runtime::Ref<gameobjects::player::Player>(player)] {
			endEffect(*owner);
		}, time));
	}
	endTime.set(commons::utils::currentTimeMillis() + time);
	for (const templates::instance_bonusatrr::InstancePenaltyAttr& instancePenaltyAttr : instanceBonusAttr->getPenaltyAttr()) {
		stats::container::StatEnum stat = instancePenaltyAttr.getStat();
		int32_t statToModified = player.getGameStats()->getStat(stat, 0)->getBase();
		int32_t value = instancePenaltyAttr.getValue();
		int32_t valueModified = instancePenaltyAttr.getFunc() == skillengine::change::Func::PERCENT ? (statToModified * value / 100) : (value);
		functions.add(stats::calc::functions::RcStatFunction<stats::calc::functions::StatAddFunction>::create(stat, valueModified, true));
	}
	std::vector<runtime::Ptr<stats::calc::functions::IStatFunction>> added = functions.snapshot();
	player.getGameStats()->addEffect(runtime::Ptr<stats::calc::StatOwner>(static_cast<stats::calc::StatOwner&>(*this)), added);
}

// Java InstanceBuff.java:61-68
void InstanceBuff::endEffect(gameobjects::player::Player& player) {
	functions.clear();
	if (isActive()) {
		task.get()->cancel(true);
	}
	player.getGameStats()->endEffect(*this);
	notify(player);
}

// Java InstanceBuff.java:70-81
void InstanceBuff::notify(gameobjects::player::Player& player) {
	using network::aion::serverpackets::SM_INSTANCE_SCORE;
	runtime::Ptr<world::WorldMapInstance> wmi = player.getWorldMapInstance();
	runtime::Ptr<instancescore::InstanceScore> score = wmi->getInstanceHandler()->getInstanceScore();
	if (runtime::Ptr<instancescore::HarmonyArenaScore> harmonyScore = runtime::as<instancescore::HarmonyArenaScore>(score)) {
		wmi->forEachPlayer([&wmi, &harmonyScore, &player](gameobjects::player::Player& p) {
			runtime::Ref<network::aion::instanceinfo::HarmonyScoreWriter> writer =
				network::aion::instanceinfo::HarmonyScoreWriter::create(*harmonyScore, InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS, player);
			utils::PacketSendUtility::sendPacket(p, SM_INSTANCE_SCORE(wmi->getMapId(), *writer, harmonyScore->getTime()));
		});
	} else if (runtime::Ptr<instancescore::PvPArenaScore> arenaScore = runtime::as<instancescore::PvPArenaScore>(score)) {
		wmi->forEachPlayer([&wmi, &arenaScore](gameobjects::player::Player& p) {
			runtime::Ref<network::aion::instanceinfo::ArenaScoreWriter> writer =
				network::aion::instanceinfo::ArenaScoreWriter::create(*arenaScore, p.getObjectId(), false);
			utils::PacketSendUtility::sendPacket(p, SM_INSTANCE_SCORE(wmi->getMapId(), *writer));
		});
	}
	utils::PacketSendUtility::sendPacket(player,
		network::aion::serverpackets::SM_ABNORMAL_STATE(std::vector<runtime::Ptr<skillengine::model::Effect>>(), player.getEffectController()->getAbnormals(), 0)); // Java: Collections.emptyList()
}

// Java InstanceBuff.java:83-85
int32_t InstanceBuff::getRemainingTime() {
	return static_cast<int32_t>(std::max<int64_t>(0, endTime.get() - commons::utils::currentTimeMillis()));
}

// Java InstanceBuff.java:102-104
bool InstanceBuff::isActive() {
	runtime::Ptr<runtime::Future> current = task.get();
	return current != nullptr && !current->isDone();
}

} // namespace aion::gameserver::model::instance
