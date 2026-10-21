#include "aion/gameserver/custom/instance/InstanceScoring.h"

#include <algorithm>
#include <format>

#include "aion/gameserver/configs/main/RuntimeConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/InstanceProgressionType.h"
#include "aion/gameserver/model/instance/instancescore/DarkPoetaScore.h"
#include "aion/gameserver/model/templates/npc/NpcRating.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/aion/instanceinfo/DarkPoetaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/WorldMapInstance.h"

namespace aion::gameserver::custom::instance {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using utils::PacketSendUtility;

InstanceScoring::InstanceScoring(Settings settingsValue, Clock clockValue)
	: settings(std::move(settingsValue)), clock(std::move(clockValue)), panelScore(model::instance::instancescore::DarkPoetaScore::create()) {
	panelScore->setInstanceProgressionType(model::instance::InstanceProgressionType::START_PROGRESS);
}

InstanceScoring::~InstanceScoring() = default;

runtime::Ref<InstanceScoring> InstanceScoring::create(Settings settings, Clock clock) {
	return runtime::makeRef<InstanceScoring>(std::move(settings), std::move(clock));
}

void InstanceScoring::setTitle(std::string value) {
	title.set(std::move(value));
}

bool InstanceScoring::start(world::WorldMapInstance& instance) {
	if (!startMillis.compareAndSet(0, clock()))
		return false;
	PacketSendUtility::broadcastToMap(instance, network::aion::serverpackets::SM_QUEST_ACTION(0, settings.timeLimitSeconds));
	sendPanel(instance);
	return true;
}

int64_t InstanceScoring::getTimeLeftMillis() const {
	const int64_t start = startMillis.get();
	if (start == 0)
		return int64_t{settings.timeLimitSeconds} * 1000;
	const int64_t end = endMillis.get() != 0 ? endMillis.get() : clock();
	return std::max<int64_t>(0, int64_t{settings.timeLimitSeconds} * 1000 - (end - start));
}

int32_t InstanceScoring::pointsOf(Npc& npc) const {
	if (std::ranges::find(settings.bossNpcIds, npc.getNpcId()) != settings.bossNpcIds.end())
		return settings.bossPoints;
	std::optional<model::templates::npc::NpcRating> rating = npc.getObjectTemplate()->getRating();
	if (rating == model::templates::npc::NpcRating::HERO || rating == model::templates::npc::NpcRating::LEGENDARY)
		return settings.bossPoints;
	if (rating == model::templates::npc::NpcRating::ELITE)
		return settings.elitePoints;
	return settings.normalPoints;
}

int32_t InstanceScoring::onKill(world::WorldMapInstance& instance, Npc& npc) {
	if (!isStarted() || isFinished())
		return 0;
	const int32_t earned = pointsOf(npc);
	points.addAndGet(earned);
	panelScore->addNpcKill();
	panelScore->addPoints(earned);
	PacketSendUtility::broadcastToMap(instance,
		network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_GET_SCORE(npc.getObjectTemplate()->getL10n(), earned));
	sendPanel(instance);
	if (std::ranges::find(settings.finalBossNpcIds, npc.getNpcId()) != settings.finalBossNpcIds.end())
		finish(instance, true);
	return earned;
}

int32_t InstanceScoring::rankOf(int64_t timeLeftMillis, int32_t totalPoints, bool finalBossKilled) const {
	if (!finalBossKilled || timeLeftMillis <= 0)
		return 8;
	const double limit = static_cast<double>(settings.timeLimitSeconds) * 1000.0;
	for (int32_t r = 0; r < 4; r++) {
		if (static_cast<double>(timeLeftMillis) > settings.timeFractions[r] * limit && totalPoints >= settings.pointThresholds[r])
			return r + 1;
	}
	return 5;
}

std::string InstanceScoring::resultLine(int32_t rankValue, int32_t totalPoints, int64_t timeLeftMillis) const {
	const int64_t seconds = timeLeftMillis / 1000;
	return std::format("[{}] Rank {} - {} points, {}:{:02} left", title.get().empty() ? std::string("Instance") : std::string(title.get()), rankValue,
		totalPoints, seconds / 60, seconds % 60);
}

int32_t InstanceScoring::finish(world::WorldMapInstance& instance, bool finalBossKilled) {
	if (!isStarted() || !finished.compareAndSet(false, true))
		return 0;
	endMillis.set(clock());
	const int64_t timeLeft = getTimeLeftMillis();
	const int32_t total = points.get();
	const int32_t rankValue = rankOf(timeLeft, total, finalBossKilled);
	rank.set(rankValue);
	panelScore->setRank(rankValue);
	panelScore->setInstanceProgressionType(model::instance::InstanceProgressionType::END_PROGRESS);
	PacketSendUtility::broadcastToMap(instance, network::aion::serverpackets::SM_QUEST_ACTION(0, 0));
	sendPanel(instance);
	const std::string line = resultLine(rankValue, total, timeLeft);
	const std::vector<Reward>& rewards = settings.rewards[static_cast<size_t>(rankValue - 1)];
	const RewardGiver give = rewardGiver.get();
	instance.forEachPlayer([&line, &rewards, give](Player& player) {
		PacketSendUtility::sendMessage(player, line); // GOLDEN_YELLOW: the chat log
		for (const Reward& reward : rewards)
			give(player, reward.first, reward.second);
	});
	return rankValue;
}

void InstanceScoring::setRewardGiverForTests(RewardGiver giver) {
	rewardGiver.set(giver != nullptr ? giver : &InstanceScoring::addItem);
}

void InstanceScoring::addItem(Player& player, int32_t itemId, int64_t count) {
	services::item::ItemService::addItem(player, itemId, count, true);
}

void InstanceScoring::sendPanel(world::WorldMapInstance& instance) {
	if (!configs::main::RuntimeConfig::INSTANCE_SCORING_DP_PANEL.load())
		return;
	runtime::Ref<network::aion::instanceinfo::DarkPoetaScoreWriter> writer = network::aion::instanceinfo::DarkPoetaScoreWriter::create(*panelScore);
	PacketSendUtility::broadcastToMap(instance,
		network::aion::serverpackets::SM_INSTANCE_SCORE(settings.panelMapId, *writer, static_cast<int32_t>(getTimeLeftMillis())));
}

} // namespace aion::gameserver::custom::instance
