#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"

#include <cmath>
#include <limits>

#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::network::aion::instanceinfo {

namespace {

/** Java (int) floatValue: NaN is 0, out-of-range values saturate */
int32_t javaFloatToInt(float value) noexcept {
	if (std::isnan(value))
		return 0;
	if (value >= 2147483648.0f)
		return std::numeric_limits<int32_t>::max();
	if (value <= -2147483648.0f)
		return std::numeric_limits<int32_t>::min();
	return static_cast<int32_t>(value);
}

} // namespace

ArenaScoreWriter::ArenaScoreWriter(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectIdValue, bool rewardTableValue)
	: InstanceScoreWriter(score), ownerObjectId(ownerObjectIdValue), rewardTable(rewardTableValue) {
}

ArenaScoreWriter::~ArenaScoreWriter() = default;

runtime::Ref<ArenaScoreWriter> ArenaScoreWriter::create(model::instance::instancescore::PvPArenaScore& score, int32_t ownerObjectIdValue,
	bool rewardTableValue) {
	return runtime::makeRef<ArenaScoreWriter>(score, ownerObjectIdValue, rewardTableValue);
}

model::instance::instancescore::PvPArenaScore& ArenaScoreWriter::score() const {
	return static_cast<model::instance::instancescore::PvPArenaScore&>(*instanceScore); // constructed from a PvPArenaScore
}

// Java ArenaScoreWriter.java:25-34
void ArenaScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	writePlayerScores(buf);
	writeOwnerRewards(buf);
	writeD(buf, 0); // Another instance buff id, possibly 'stageend_buff'
	writeD(buf, 0); // unk
	writeD(buf, score().getRound()); // round
	writeD(buf, score().getUpperScoreCap()); // cap points
	writeD(buf, 3); // possible rounds
	writeD(buf, rewardTable ? 1 : 0); // possible reward table
}

// Java ArenaScoreWriter.java:36-57
void ArenaScoreWriter::writePlayerScores(commons::utils::ByteBuffer& buf) {
	int32_t playerCount = 0;
	for (const runtime::Ptr<model::instance::playerreward::PvPArenaPlayerReward>& apr : score().getPlayerRewards()) {
		writeD(buf, apr->getOwnerId());
		writeD(buf, apr->getPvPKills());
		writeD(buf, score().isRewarded() ? apr->getScorePoints() : apr->getPoints());
		writeD(buf, 0); // unk
		writeC(buf, 0); // unk
		writeC(buf, model::getClassId(apr->getPlayerClass()));
		writeC(buf, 1); // unk
		writeC(buf, score().getRank(*apr)); // top position
		writeD(buf, apr->getRemainingTime()); // instance buff time, also controls shield effect
		writeD(buf, score().isRewarded() ? apr->getTimeBonus() : 0);
		writeD(buf, apr->hasBoostMorale() ? score().getBuffId() : 0); // Instance Buff ID
		writeD(buf, 0); // unk
		writeH(buf, score().isRewarded() ? static_cast<int16_t>(javaFloatToInt(apr->getParticipation() * 100)) : 0); // participation
		writeS(buf, apr->getPlayerName(), 54);
		playerCount++;
	}
	if (playerCount < 12)
		skip(buf, 92 * (12 - playerCount)); // spaces (Java: writeB(buf, new byte[...]))
}

// Java ArenaScoreWriter.java:59-71
void ArenaScoreWriter::writeOwnerRewards(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::instance::playerreward::PvPArenaPlayerReward> arenaReward = score().getPlayerReward(ownerObjectId);
	if (score().isRewarded() && score().canReward() && arenaReward != nullptr) {
		writeReward(buf, *arenaReward->getAp());
		writeReward(buf, *arenaReward->getGp());
		writeReward(buf, *arenaReward->getCrucibleInsignia());
		writeReward(buf, *arenaReward->getCourageInsignia());
		writeSimpleReward(buf, arenaReward->getRewardItem1().get());
		writeSimpleReward(buf, arenaReward->getRewardItem2().get());
	} else {
		skip(buf, 72); // Java: writeB(buf, new byte[72])
	}
}

// Java ArenaScoreWriter.java:73-79
void ArenaScoreWriter::writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	if (rewardItem.itemId() != 0)
		writeD(buf, rewardItem.itemId());
	writeD(buf, rewardItem.baseCount());
	writeD(buf, rewardItem.scoreCount());
	writeD(buf, rewardItem.rankingCount());
}

void ArenaScoreWriter::writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem) {
	network::detail::writeSimpleReward(buf, rewardItem);
}

} // namespace aion::gameserver::network::aion::instanceinfo
