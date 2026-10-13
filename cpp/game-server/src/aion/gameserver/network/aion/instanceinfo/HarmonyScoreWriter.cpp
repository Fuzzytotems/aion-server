#include "aion/gameserver/network/aion/instanceinfo/HarmonyScoreWriter.h"

#include <vector>

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/instancescore/InstanceScore.h"
#include "aion/gameserver/network/detail/InstanceInfoWriting.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/instance/InstanceScoreTypeInfo.h"
#include "aion/gameserver/model/instance/instancescore/HarmonyArenaScore.h"
#include "aion/gameserver/model/instance/playerreward/HarmonyGroupReward.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/ArenaRewardItem.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::network::aion::instanceinfo {

// Java HarmonyScoreWriter.java:22-26
HarmonyScoreWriter::HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType typeValue,
	model::gameobjects::player::Player& owner)
	: InstanceScoreWriter(reward), type(typeValue), playerObjId(owner.getObjectId()) {
}

// Java HarmonyScoreWriter.java:28-31 (playerObjId keeps the Java default 0)
HarmonyScoreWriter::HarmonyScoreWriter(model::instance::instancescore::HarmonyArenaScore& reward, model::instance::InstanceScoreType typeValue)
	: InstanceScoreWriter(reward), type(typeValue), playerObjId(0) {
}

HarmonyScoreWriter::~HarmonyScoreWriter() = default;

runtime::Ref<HarmonyScoreWriter> HarmonyScoreWriter::create(model::instance::instancescore::HarmonyArenaScore& reward,
	model::instance::InstanceScoreType typeValue, model::gameobjects::player::Player& owner) {
	return runtime::makeRef<HarmonyScoreWriter>(reward, typeValue, owner);
}

runtime::Ref<HarmonyScoreWriter> HarmonyScoreWriter::create(model::instance::instancescore::HarmonyArenaScore& reward,
	model::instance::InstanceScoreType typeValue) {
	return runtime::makeRef<HarmonyScoreWriter>(reward, typeValue);
}

model::instance::instancescore::HarmonyArenaScore& HarmonyScoreWriter::score() const {
	return static_cast<model::instance::instancescore::HarmonyArenaScore&>(*instanceScore); // constructed from a HarmonyArenaScore
}

// Java HarmonyScoreWriter.java:33-126
void HarmonyScoreWriter::writeMe(commons::utils::ByteBuffer& buf) {
	using model::instance::InstanceScoreType;
	using model::instance::playerreward::HarmonyGroupReward;
	using model::instance::playerreward::PvPArenaPlayerReward;
	runtime::Ptr<HarmonyGroupReward> harmonyGroupReward = score().getGroupReward(playerObjId);
	writeC(buf, model::instance::getId(type));
	switch (type) {
		case InstanceScoreType::UPDATE_INSTANCE_PROGRESS:
			writeD(buf, 0);
			writeD(buf, score().getRound());
			break;
		case InstanceScoreType::INIT_PLAYER:
			if (harmonyGroupReward == nullptr) {
				skip(buf, 64); // Java: writeB(buf, new byte[64])
				return;
			}

			writeD(buf, harmonyGroupReward->getOwnerId());
			writeS(buf, harmonyGroupReward->getAGPlayer(playerObjId)->name(), 52); // playerName
			writeD(buf, harmonyGroupReward->getGrpObjectId()); // groupObj
			writeD(buf, playerObjId); // memberObj
			break;
		case InstanceScoreType::UPDATE_PLAYER_BUFF_STATUS: {
			runtime::Ptr<PvPArenaPlayerReward> reward = score().getPlayerReward(playerObjId);
			if (reward == nullptr) {
				skip(buf, 16); // Java: writeB(buf, new byte[16])
				return;
			}

			writeD(buf, reward->getRemainingTime()); // buffTime
			writeD(buf, reward->hasBoostMorale() ? score().getBuffId() : 0); // Buff ID
			writeD(buf, 0);
			writeD(buf, playerObjId); // memberObj
			break;
		}
		case InstanceScoreType::SHOW_REWARD: {
			runtime::Ptr<PvPArenaPlayerReward> playerReward = score().getPlayerReward(playerObjId);
			if (playerReward == nullptr) {
				skip(buf, 72); // Java: writeB(buf, new byte[72])
				return;
			}

			writePoints(buf, *playerReward->getAp());
			writePoints(buf, *playerReward->getGp());
			writeReward(buf, *playerReward->getCourageInsignia());
			writeReward(buf, *playerReward->getCrucibleInsignia());
			writeSimpleReward(buf, playerReward->getRewardItem1().get());
			writeSimpleReward(buf, playerReward->getRewardItem2().get());
			break;
		}
		case InstanceScoreType::UPDATE_INSTANCE_BUFFS_AND_SCORE: {
			writeD(buf, score().getLowerScoreCap());
			writeD(buf, score().getUpperScoreCap()); // capPoints
			writeD(buf, 3); // possible rounds
			writeD(buf, 1);
			writeD(buf, 0); // Global Buff Id
			writeD(buf, 0);
			writeD(buf, 0);
			writeD(buf, score().getRound()); // round
			std::vector<runtime::Ptr<HarmonyGroupReward>> groups = score().getHarmonyGroupInside();
			writeC(buf, static_cast<int32_t>(groups.size())); // size
			int32_t groupIndex = 0;
			for (const runtime::Ptr<HarmonyGroupReward>& group : groups) {
				writeC(buf, score().getRank(*group));
				writeD(buf, group->getPvPKills());
				writeD(buf, group->getPoints()); // groupScore
				writeD(buf, group->getGrpObjectId()); // groupObj
				std::vector<runtime::Ptr<model::gameobjects::player::Player>> members = score().getPlayersInside(*group);
				writeC(buf, static_cast<int32_t>(members.size()));
				int32_t i = 0;
				for (const runtime::Ptr<model::gameobjects::player::Player>& p : members) {
					runtime::Ptr<PvPArenaPlayerReward> apr = score().getPlayerReward(p->getObjectId());
					writeD(buf, 0);
					writeD(buf, apr->getRemainingTime());
					writeD(buf, 0); // Probably Buff id?
					writeC(buf, groupIndex); // groupIndex, necessary for group names
					writeC(buf, i); // memberNr
					writeH(buf, 0);
					writeS(buf, p->getName(), 52); // playerName
					writeD(buf, p->getObjectId()); // memberObj
					i++;
				}
				groupIndex++;
			}
			break;
		}
		case InstanceScoreType::UPDATE_RANK:
			if (harmonyGroupReward == nullptr) {
				skip(buf, 13); // Java: writeB(buf, new byte[13])
				return;
			}

			writeC(buf, score().getRank(*harmonyGroupReward));
			writeD(buf, harmonyGroupReward->getPvPKills()); // kills
			writeD(buf, harmonyGroupReward->getPoints()); // groupScore
			writeD(buf, harmonyGroupReward->getGrpObjectId()); // groupObj
			break;
		default: // Java: the other constants write only the type
			break;
	}
}

// Java HarmonyScoreWriter.java:128-132
void HarmonyScoreWriter::writePoints(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	writeD(buf, rewardItem.baseCount());
	writeD(buf, rewardItem.scoreCount());
	writeD(buf, rewardItem.rankingCount());
}

// Java HarmonyScoreWriter.java:134-139
void HarmonyScoreWriter::writeReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::ArenaRewardItem& rewardItem) {
	writeD(buf, rewardItem.itemId());
	writeD(buf, rewardItem.baseCount());
	writeD(buf, rewardItem.scoreCount());
	writeD(buf, rewardItem.rankingCount());
}

void HarmonyScoreWriter::writeSimpleReward(commons::utils::ByteBuffer& buf, const model::templates::rewards::RewardItem* rewardItem) {
	network::detail::writeSimpleReward(buf, rewardItem);
}

} // namespace aion::gameserver::network::aion::instanceinfo
