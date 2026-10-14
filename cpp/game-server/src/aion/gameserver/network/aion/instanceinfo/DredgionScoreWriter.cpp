#include "aion/gameserver/network/aion/instanceinfo/DredgionScoreWriter.h"

#include <array>
#include <string>
#include <utility>

#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/InstanceProgressionTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/model/instance/instancescore/PvpInstanceScore.h"
#include "aion/gameserver/model/instance/playerreward/PvpInstancePlayerReward.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java DredgionScoreWriter.java:20-24
DredgionScoreWriter::DredgionScoreWriter(model::instance::instancescore::PvpInstanceScore& reward,
	std::vector<runtime::Ref<model::gameobjects::player::Player>> playersValue,
	std::vector<runtime::Ref<model::instance::DredgionRoom>> dredgionRoomsValue)
	: InstanceScoreWriter(reward) {
	players.addAll(playersValue);
	dredgionRooms.addAll(dredgionRoomsValue);
}

DredgionScoreWriter::~DredgionScoreWriter() = default;

runtime::Ref<DredgionScoreWriter> DredgionScoreWriter::create(model::instance::instancescore::PvpInstanceScore& reward,
                                                              std::vector<runtime::Ref<model::gameobjects::player::Player>> playersValue,
                                                              std::vector<runtime::Ref<model::instance::DredgionRoom>> dredgionRoomsValue) {
	return runtime::makeRef<DredgionScoreWriter>(reward, std::move(playersValue), std::move(dredgionRoomsValue));
}

model::instance::instancescore::PvpInstanceScore& DredgionScoreWriter::score() const {
	return static_cast<model::instance::instancescore::PvpInstanceScore&>(*instanceScore); // constructed from a PvpInstanceScore
}

// Java DredgionScoreWriter.java:26-36
void DredgionScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	fillTableWithGroup(buf, model::Race::ELYOS);
	fillTableWithGroup(buf, model::Race::ASMODIANS);
	int32_t elyosScore = score().getElyosPoints();
	int32_t asmosScore = score().getAsmodiansPoints();
	writeD(buf, model::instance::isEndProgress(score().getInstanceProgressionType()) ? (asmosScore > elyosScore ? 1 : 0) : 255);
	writeD(buf, elyosScore);
	writeD(buf, asmosScore);
	for (const runtime::Ptr<model::instance::DredgionRoom>& d : dredgionRooms.snapshot())
		writeC(buf, d->getState());
}

// Java DredgionScoreWriter.java:38-68
void DredgionScoreWriter::fillTableWithGroup(commons::utils::ByteBuffer& buf, model::Race race) {
	int32_t count = 0;
	for (const runtime::Ptr<model::gameobjects::player::Player>& player : players.snapshot()) {
		if (race != player->getRace()) {
			continue;
		}
		runtime::Ptr<model::instance::playerreward::PvpInstancePlayerReward> playerReward =
			runtime::cast<model::instance::playerreward::PvpInstancePlayerReward>(score().getPlayerReward(player->getObjectId())); // Java: T
		if (playerReward == nullptr) // Java: playerReward.getOwnerId() of no reward
			throw runtime::NullPointerException("PvpInstanceScore.getPlayerReward(" + std::to_string(player->getObjectId()) + ")");
		writeD(buf, playerReward->getOwnerId()); // playerObjectId
		writeD(buf, utils::stats::id(player->getAbyssRank()->getRank())); // playerRank
		writeD(buf, playerReward->getPvPKills()); // pvpKills
		writeD(buf, playerReward->getMonsterKills()); // monsterKills
		writeD(buf, playerReward->getCapturedZones()); // captured
		writeD(buf, playerReward->getPoints()); // playerScore

		if (model::instance::isEndProgress(score().getInstanceProgressionType())) {
			writeD(buf, playerReward->getBonusAp() + playerReward->getBaseAp()); // Client needs to know the sum here
			writeD(buf, playerReward->getBaseAp());
		} else {
			writeB(buf, std::array<uint8_t, 8>{});
		}

		writeC(buf, model::getClassId(player->getPlayerClass())); // playerClass
		writeC(buf, 0); // unk
		writeS(buf, player->getName(), 54); // playerName
		count++;
	}
	if (count < 6) {
		network::detail::writeEmptyData(buf, 88, 6 - count); // spaces
	}
}

} // namespace aion::gameserver::network::aion::instanceinfo
