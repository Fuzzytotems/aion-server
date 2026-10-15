#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"

#include <algorithm>
#include <limits>

#include "aion/commons/utils/Rnd.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/instanceposition/ChaosInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/DisciplineInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/GloryInstancePosition.h"
#include "aion/gameserver/model/instance/instanceposition/HarmonyInstancePosition.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"

namespace aion::gameserver::model::instance::instancescore {

using world::WorldMapType;

// Java PvPArenaScore.java:41-64 (the switch: bonusTime, positionSize, instancePosition; the default arm leaves them 0 and null)
PvPArenaScore::Setup PvPArenaScore::setupOf(std::optional<WorldMapType> wmt) {
	if (!wmt) // Java: case null
		return {0, 0, nullptr};
	switch (*wmt) {
		case WorldMapType::DISCIPLINE_TRAINING_GROUNDS:
		case WorldMapType::ARENA_OF_DISCIPLINE:
			return {8100, 4, instanceposition::DisciplineInstancePosition::create()};
		case WorldMapType::ARENA_OF_GLORY:
			return {10800, 8, instanceposition::GloryInstancePosition::create()};
		case WorldMapType::HARMONY_TRAINING_GROUNDS:
		case WorldMapType::ARENA_OF_HARMONY:
			return {12000, 12, instanceposition::HarmonyInstancePosition::create()};
		case WorldMapType::CHAOS_TRAINING_GROUNDS:
		case WorldMapType::ARENA_OF_CHAOS:
			return {12000, 12, instanceposition::ChaosInstancePosition::create()};
		default:
			return {0, 0, nullptr};
	}
}

PvPArenaScore::PvPArenaScore(world::WorldMapInstance& instanceValue) : PvPArenaScore(instanceValue, setupOf(world::getWorldMapType(instanceValue.getMapId()))) {
}

// Java PvPArenaScore.java:32-72
PvPArenaScore::PvPArenaScore(world::WorldMapInstance& instanceValue, const Setup& setup)
	: bonusTime(setup.bonusTime), buffId(static_cast<int8_t>(isSoloArena(world::getWorldMapType(instanceValue.getMapId())) ? 8 : 7)),
	  instancePosition(setup.instancePosition) {
	this->instance.set(runtime::Ref<world::WorldMapInstance>(instanceValue));

	std::optional<WorldMapType> wmt = world::getWorldMapType(instanceValue.getMapId());
	bool isSolo = isSoloArena(wmt);
	for (int32_t z : isSolo ? std::vector<int32_t>{1, 2, 3, 4} : std::vector<int32_t>{1, 2, 3, 4, 5, 6}) // Java: Collections.addAll(zones, ...)
		zones.add(z);

	if (instancePosition == nullptr) // Java: case null, default -> return
		return;

	instancePosition->initialize(instanceValue.getMapId(), instanceValue.getInstanceId());
	for (int32_t i = 1; i <= setup.positionSize; i++) {
		positions.put(i, false);
	}
	setRndZone();
}

PvPArenaScore::~PvPArenaScore() = default;

runtime::Ref<PvPArenaScore> PvPArenaScore::create(world::WorldMapInstance& instance) {
	return runtime::makeRef<PvPArenaScore>(instance);
}

runtime::Ptr<playerreward::PvPArenaPlayerReward> PvPArenaScore::getPlayerReward(int32_t objectId) {
	runtime::Ptr<playerreward::InstancePlayerReward> reward = InstanceScore::getPlayerReward(objectId);
	runtime::Ptr<playerreward::PvPArenaPlayerReward> arenaReward = runtime::as<playerreward::PvPArenaPlayerReward>(reward);
	if (reward != nullptr && arenaReward == nullptr)
		throw runtime::ClassCastException("InstancePlayerReward cannot be cast to PvPArenaPlayerReward");
	return arenaReward;
}

std::vector<runtime::Ptr<playerreward::PvPArenaPlayerReward>> PvPArenaScore::getPlayerRewards() {
	std::vector<runtime::Ptr<playerreward::PvPArenaPlayerReward>> rewards;
	for (const runtime::Ptr<playerreward::InstancePlayerReward>& reward : InstanceScore::getPlayerRewards()) {
		runtime::Ptr<playerreward::PvPArenaPlayerReward> arenaReward = runtime::as<playerreward::PvPArenaPlayerReward>(reward);
		if (arenaReward == nullptr)
			throw runtime::ClassCastException("InstancePlayerReward cannot be cast to PvPArenaPlayerReward");
		rewards.push_back(arenaReward);
	}
	return rewards;
}

// Java PvPArenaScore.java:74-76
bool PvPArenaScore::isSoloArena(std::optional<WorldMapType> wmt) const {
	return wmt == WorldMapType::DISCIPLINE_TRAINING_GROUNDS || wmt == WorldMapType::ARENA_OF_DISCIPLINE;
}

// Java PvPArenaScore.java:78-81
void PvPArenaScore::setRndZone() {
	int32_t index = commons::utils::Rnd::nextInt(zones.size());
	zone.set(zones.removeAt(index));
}

// Java PvPArenaScore.java:83-91
std::vector<int32_t> PvPArenaScore::getFreePositions() {
	std::vector<int32_t> p;
	for (int32_t key : positions.keySet()) {
		if (!positions.get(key).value()) { // Java: positions.get(key) unboxed
			p.push_back(key);
		}
	}
	return p;
}

// Java PvPArenaScore.java:93-105 (synchronized)
void PvPArenaScore::setRndPosition(int32_t objectId) {
	SYNCHRONIZED(*this) {
		runtime::Ptr<playerreward::PvPArenaPlayerReward> reward = getPlayerReward(objectId);
		int32_t position = reward->getPosition();
		if (position != 0) {
			clearPosition(position, false);
		}
		std::vector<int32_t> freePositions = getFreePositions();
		if (!freePositions.empty()) {
			int32_t key = commons::utils::Rnd::get(freePositions);
			clearPosition(key, true);
			reward->setPosition(key);
		}
	}
}

// Java PvPArenaScore.java:107-109 (synchronized)
void PvPArenaScore::clearPosition(int32_t position, bool result) {
	SYNCHRONIZED(*this) {
		positions.put(position, result);
	}
}

// Java PvPArenaScore.java:115-119
void PvPArenaScore::incrementRound() {
	this->round.set(this->round.get() + 1);
	if (round.get() > 3)
		round.set(3);
}

// Java PvPArenaScore.java:121-127
bool PvPArenaScore::regPlayerReward(gameobjects::player::Player& player) {
	if (!containsPlayer(player.getObjectId())) {
		addPlayerReward(*playerreward::PvPArenaPlayerReward::create(player, bonusTime, buffId));
		return true;
	}
	return false;
}

// Java PvPArenaScore.java:129-134
void PvPArenaScore::portToPosition(gameobjects::player::Player& player) {
	int32_t objectId = player.getObjectId();
	regPlayerReward(player);
	setRndPosition(objectId);
	std::optional<int32_t> currentZone = zone.get();
	if (!currentZone) // Java: the Integer zone unboxed
		throw runtime::NullPointerException("PvPArenaScore.zone");
	if (instancePosition == nullptr)
		throw runtime::NullPointerException("PvPArenaScore.instancePosition");
	instancePosition->port(player, *currentZone, getPlayerReward(objectId)->getPosition());
}

// Java PvPArenaScore.java:136-146
int32_t PvPArenaScore::getRank(playerreward::PvPArenaPlayerReward& reward) {
	std::vector<runtime::Ptr<playerreward::PvPArenaPlayerReward>> sortedByPoints = getPlayerRewards();
	std::stable_sort(sortedByPoints.begin(), sortedByPoints.end(),
		[](const auto& a, const auto& b) { return a->getScorePoints() < b->getScorePoints(); });

	int32_t rank = -1;
	for (const runtime::Ptr<playerreward::PvPArenaPlayerReward>& r : sortedByPoints) {
		if (r->getScorePoints() >= reward.getScorePoints())
			rank++;
	}
	return rank;
}

// Java PvPArenaScore.java:148-153 (IntSummaryStatistics: an empty stream has max Integer.MIN_VALUE and min Integer.MAX_VALUE)
bool PvPArenaScore::reachedScoreGap() {
	int32_t maxPoints = std::numeric_limits<int32_t>::min();
	int32_t minPoints = std::numeric_limits<int32_t>::max();
	for (const runtime::Ptr<playerreward::PvPArenaPlayerReward>& reward : getPlayerRewards()) {
		maxPoints = std::max(maxPoints, reward->getPoints());
		minPoints = std::min(minPoints, reward->getPoints());
	}
	int32_t gap = static_cast<int32_t>(static_cast<uint32_t>(maxPoints) - static_cast<uint32_t>(minPoints)); // Java int arithmetic wraps
	return gap >= maxScoreGap.get();
}

// Java PvPArenaScore.java:155-157
int32_t PvPArenaScore::getTotalPoints() {
	int32_t sum = 0;
	for (const runtime::Ptr<playerreward::PvPArenaPlayerReward>& reward : getPlayerRewards())
		sum = static_cast<int32_t>(static_cast<uint32_t>(sum) + static_cast<uint32_t>(reward->getScorePoints()));
	return sum;
}

// Java PvPArenaScore.java:159-164
bool PvPArenaScore::canReward() {
	std::optional<WorldMapType> type = world::getWorldMapType(instance.get()->getMapId());
	if (!type)
		return false;
	switch (*type) {
		case WorldMapType::ARENA_OF_DISCIPLINE:
		case WorldMapType::ARENA_OF_GLORY:
		case WorldMapType::ARENA_OF_CHAOS:
		case WorldMapType::ARENA_OF_HARMONY:
			return true;
		default:
			return false;
	}
}

// Java PvPArenaScore.java:166-194
int32_t PvPArenaScore::getNpcBonusSkill(int32_t npcId) const {
	switch (npcId) {
		case 701175: // Plaza Flame Thrower
		case 701176:
		case 701177:
		case 701178:
			return 0x4E5732; // 20055 50
		case 701189: // Plaza Flame Thrower
		case 701190:
		case 701191:
		case 701192:
			return 0x4FDB3C; // 20443 60
		case 701317: // Illusion of Hope level 47
			return 0x4f8532; // 20357 50
		case 701318: // Illusion of Hope level 51
			return 0x4f8537; // 20357 55
		case 701319: // Illusion of Hope level 56
			return 0x4f853C; // 20357 60
		case 701220: // Blesed Relic
			return 0x4E5537; // 20053 55 //20068, 20072
		case 207118:
		case 207119:
			return 0x50C101; // 20673 1
		case 207100:
			return 0x50BE01; // 20670 1
		default:
			return 0;
	}
}

// Java PvPArenaScore.java:196-206
int32_t PvPArenaScore::getTime() {
	int64_t result = commons::utils::currentTimeMillis() - instanceTime.get();
	if (isRewarded()) {
		return 0;
	}
	if (result < 120000) {
		return static_cast<int32_t>(120000 - result);
	} else {
		return static_cast<int32_t>(180000 * static_cast<int64_t>(getRound()) - (result - 120000));
	}
}

// Java PvPArenaScore.java:236-238
void PvPArenaScore::setInstanceStartTime() {
	this->instanceTime.set(commons::utils::currentTimeMillis());
}

// Java PvPArenaScore.java:244-248
void PvPArenaScore::clear() {
	InstanceScore::clear();
	positions.clear();
}

} // namespace aion::gameserver::model::instance::instancescore
