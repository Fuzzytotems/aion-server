#include "aion/gameserver/network/aion/instanceinfo/PvpInstanceScoreWriter.h"

#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/instance/InstanceScoreTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java PvpInstanceScoreWriter.java:25-28
PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
	model::instance::InstanceScoreType instanceScoreTypeValue)
	: InstanceScoreWriter(reward), instanceScoreType(instanceScoreTypeValue), race(), objectId(0), status(0) {
}

// Java PvpInstanceScoreWriter.java:30-34
PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
	model::instance::InstanceScoreType instanceScoreTypeValue, model::Race raceValue)
	: InstanceScoreWriter(reward), instanceScoreType(instanceScoreTypeValue), race(raceValue), objectId(0), status(0) {
}

// Java PvpInstanceScoreWriter.java:36-41
PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
	model::instance::InstanceScoreType instanceScoreTypeValue, int32_t objectIdValue, int32_t statusValue)
	: InstanceScoreWriter(reward), instanceScoreType(instanceScoreTypeValue), race(), objectId(objectIdValue), status(statusValue) {
}

// Java PvpInstanceScoreWriter.java:43-47
PvpInstanceScoreWriter::PvpInstanceScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
	model::instance::InstanceScoreType instanceScoreTypeValue, std::vector<runtime::Ref<model::gameobjects::player::Player>> participantsValue)
	: InstanceScoreWriter(reward), instanceScoreType(instanceScoreTypeValue), race(), objectId(0), status(0) {
	participants.addAll(participantsValue); // Java stores the caller's list; nothing modifies it after construction
}

PvpInstanceScoreWriter::~PvpInstanceScoreWriter() = default;

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue,
                                                                    model::Race raceValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, raceValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue, int32_t objectIdValue,
                                                                    int32_t statusValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, objectIdValue, statusValue);
}

runtime::Ref<PvpInstanceScoreWriter> PvpInstanceScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                                    model::instance::InstanceScoreType instanceScoreTypeValue,
                                                                    std::vector<runtime::Ref<model::gameobjects::player::Player>> participantsValue) {
	return runtime::makeRef<PvpInstanceScoreWriter>(reward, instanceScoreTypeValue, std::move(participantsValue));
}

model::instance::instancescore::PvpInstanceScore& PvpInstanceScoreWriter::score() const {
	return static_cast<model::instance::instancescore::PvpInstanceScore&>(*instanceScore); // constructed from a PvpInstanceScore
}

runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> PvpInstanceScoreWriter::rewardOf(int32_t playerObjectId) const {
	return runtime::cast<model::instance::playerreward::PvpInstancePlayerReward>(score().getPlayerReward(playerObjectId)); // Java: T
}

std::vector<runtime::Ptr<model::gameobjects::player::Player>> PvpInstanceScoreWriter::participantsOf(model::Race participantRace) const {
	std::vector<runtime::Ptr<model::gameobjects::player::Player>> players;
	for (const runtime::Ptr<model::gameobjects::player::Player>& p : participants.snapshot())
		if (p->getRace() == participantRace)
			players.push_back(p);
	return players;
}

// Java PvpInstanceScoreWriter.java:49-77
void PvpInstanceScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	using model::instance::InstanceScoreType;
	writeC(buf, model::instance::getId(instanceScoreType));
	switch (instanceScoreType) {
		case InstanceScoreType::UPDATE_INSTANCE_PROGRESS:
			writeD(buf, score().getInstanceProgressionType() == model::instance::InstanceProgressionType::START_PROGRESS ? 0 : 2);
			break;
		case InstanceScoreType::INIT_PLAYER: {
			writeD(buf, 0); // unk
			writeD(buf, status); // player is dead: 60 else 0
			writeD(buf, objectId);
			runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> reward = rewardOf(objectId);
			if (reward == nullptr) // Java: getPlayerReward(objectId).getRace() of no reward
				throw runtime::NullPointerException("PvpInstanceScore.getPlayerReward(" + std::to_string(objectId) + ")");
			writeD(buf, model::getRaceId(reward->getRace()));
			break;
		}
		case InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS:
			writeD(buf, 0);
			writeD(buf, status); // Spawn(0) - Dead(60)
			writeD(buf, objectId); // PlayerObjectId
			break;
		case InstanceScoreType::SHOW_REWARD:
			writeShowReward(buf);
			break;
		case InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE:
			writeUpdateScoreToBuffer(buf);
			break;
		case InstanceScoreType::UPDATE_ALL_PLAYER_INFO:
			writePlayerFullData(buf);
			break;
		case InstanceScoreType::PLAYER_QUIT:
			writeD(buf, objectId);
			break;
		case InstanceScoreType::UPDATE_FACTION_SCORE: {
			if (!race) // Java: race.getRaceId() of the null race
				throw runtime::NullPointerException("PvpInstanceScoreWriter.race");
			writeC(buf, 0);
			writeD(buf, *race == model::Race::ELYOS ? score().getElyosKills() : score().getAsmodiansKills());
			writeD(buf, *race == model::Race::ELYOS ? score().getElyosPoints() : score().getAsmodiansPoints());
			writeD(buf, model::getRaceId(*race));
			writeD(buf, model::getRaceId(score().getRaceWithHighestPoints()));
			break;
		}
		default: // Java: UPDATE_RANK has no arm
			break;
	}
}

// Java PvpInstanceScoreWriter.java:79-99
void PvpInstanceScoreWriter::writeShowReward(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> reward = rewardOf(objectId);
	if (reward == nullptr) // Java: the getters of no reward
		throw runtime::NullPointerException("PvpInstanceScore.getPlayerReward(" + std::to_string(objectId) + ")");
	writeD(buf, 100); // Participation
	writeD(buf, reward->getBaseAp());
	writeD(buf, reward->getBonusAp());
	writeD(buf, reward->getBaseGp());
	writeD(buf, reward->getBonusGp());
	writeD(buf, reward->getReward1ItemId());
	writeD(buf, reward->getReward1Count());
	writeD(buf, reward->getReward1BonusCount());
	writeD(buf, reward->getReward2ItemId());
	writeD(buf, reward->getReward2Count());
	writeD(buf, reward->getReward2BonusCount());
	writeD(buf, reward->getReward3ItemId());
	writeD(buf, reward->getReward3Count());
	writeD(buf, reward->getReward4ItemId());
	writeD(buf, reward->getReward4Count());
	writeD(buf, reward->getBonusRewardItemId());
	writeD(buf, reward->getBonusRewardCount());
	writeC(buf, reward->getBonusRewardItemId() > 0 ? 1 : 0); // showBonusReward flag
}

// Java PvpInstanceScoreWriter.java:101-115
void PvpInstanceScoreWriter::writeUpdateScoreToBuffer(commons::utils::ByteBuffer& buf) {
	writeD(buf, 100); // Should be differentiated between asmos and elyos
	writePlayerBuffInfo(buf, participantsOf(model::Race::ELYOS));
	writePlayerBuffInfo(buf, participantsOf(model::Race::ASMODIANS));
	writeC(buf, 0);
	writeD(buf, score().getElyosKills());
	writeD(buf, score().getElyosPoints());
	writeD(buf, model::getRaceId(model::Race::ELYOS));
	writeD(buf, 0);
	writeC(buf, 0);
	writeD(buf, score().getAsmodiansKills());
	writeD(buf, score().getAsmodiansPoints());
	writeD(buf, model::getRaceId(model::Race::ASMODIANS));
	writeD(buf, model::getRaceId(score().getRaceWithHighestPoints()));
}

// Java PvpInstanceScoreWriter.java:117-120
void PvpInstanceScoreWriter::writePlayerFullData(commons::utils::ByteBuffer& buf) {
	writePlayerFullData(buf, participantsOf(model::Race::ELYOS));
	writePlayerFullData(buf, participantsOf(model::Race::ASMODIANS));
}

// Java PvpInstanceScoreWriter.java:122-137
void PvpInstanceScoreWriter::writePlayerFullData(commons::utils::ByteBuffer& buf,
	const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	for (const runtime::Ptr<model::gameobjects::player::Player>& player : players) { // 69 bytes per player
		runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> reward = rewardOf(player->getObjectId());
		if (reward == nullptr)
			continue;
		writeD(buf, player->getObjectId());
		writeC(buf, model::getClassId(player->getPlayerClass()));
		writeC(buf, utils::stats::id(player->getAbyssRank()->getRank()));
		writeC(buf, 0);
		writeH(buf, 0);
		writeD(buf, reward->getPvPKills());
		writeD(buf, reward->getPoints());
		writeS(buf, player->getName(), 52);
	}
	writeEmptyDataToBuffer(buf, 69, MAXIMUM_PLAYER_COUNT_PER_FACTION - static_cast<int32_t>(players.size()));
}

void PvpInstanceScoreWriter::writePlayerBuffInfo(commons::utils::ByteBuffer& buf,
                                                 const std::vector<runtime::Ptr<model::gameobjects::player::Player>>& players) {
	network::detail::writePlayerBuffInfo(buf, players, MAXIMUM_PLAYER_COUNT_PER_FACTION);
}

void PvpInstanceScoreWriter::writeEmptyDataToBuffer(commons::utils::ByteBuffer& buf, int32_t dataSize, int32_t missingPlayerCount) {
	network::detail::writeEmptyData(buf, dataSize, missingPlayerCount); // Java: new byte[negative] throws NegativeArraySizeException
}

} // namespace aion::gameserver::network::aion::instanceinfo
