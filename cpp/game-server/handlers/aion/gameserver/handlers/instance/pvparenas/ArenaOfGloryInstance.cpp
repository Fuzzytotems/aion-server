#include "aion/gameserver/handlers/instance/pvparenas/ArenaOfGloryInstance.h"

#include "aion/gameserver/model/instance/instancescore/PvPArenaScore.h"
#include "aion/gameserver/network/aion/instanceinfo/ArenaScoreWriter.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INSTANCE_SCORE.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/model/gameobjects/player/RatesInfo.h"
#include "aion/gameserver/model/instance/playerreward/PvPArenaPlayerReward.h"
#include "aion/gameserver/model/templates/rewards/RewardItem.h"

namespace aion::gameserver::handlers::instance::pvparenas {

AION_INSTANCE_HANDLER(ArenaOfGloryInstance, 300550000);

// Java ArenaOfGloryInstance.java:21-23
ArenaOfGloryInstance::ArenaOfGloryInstance(WorldMapInstance& instance) : PvPArenaInstance(instance) {
}

runtime::Ref<ArenaOfGloryInstance> ArenaOfGloryInstance::create(WorldMapInstance& instance) {
	return runtime::makeRef<ArenaOfGloryInstance>(instance);
}

// Java ArenaOfGloryInstance.java:25-30
void ArenaOfGloryInstance::onInstanceCreate() {
	pointsPerKill = 1000;
	pointsPerDeath = -200;
	PvPArenaInstance::onInstanceCreate();
}

// Java ArenaOfGloryInstance.java:32-38
int32_t ArenaOfGloryInstance::getBoostMoraleEffectDuration(int32_t rank) {
	switch (rank) {
		case 0:
		case 1:
			return 14000;
		default:
			return 15000;
	}
}

// Java ArenaOfGloryInstance.java:40-43
float ArenaOfGloryInstance::getRunnerUpScoreMod(int32_t victimRank) {
	return 4.0f;
}

// Java ArenaOfGloryInstance.java:45-49
void ArenaOfGloryInstance::sendPacket(runtime::Ptr<Player> player, std::optional<InstanceScoreType> scoreType) {
	instance->forEachPlayer([this](Player& p) {
		runtime::Ref<ArenaScoreWriter> writer = ArenaScoreWriter::create(*instanceScore.get(), p.getObjectId(), false);
		PacketSendUtility::sendPacket(p, SM_INSTANCE_SCORE(instance->getMapId(), *writer));
	});
}

// Java ArenaOfGloryInstance.java:51-63
PvPArenaInstance::BaseRewards ArenaOfGloryInstance::getBaseRewardsPerPlayer(int32_t difficultyId) {
	// Extracted values from instant_dungeon_idarenapvp.xml (Retail Leak 4.6, also verified by different videos)
	// Retail4.6, there are 4 difficultyIds - maybe they were removed
	// 1 new BaseValuesPerPlayer(700, 0, 310, 27);
	// 2 new BaseValuesPerPlayer(900, 0, 425, 50);
	// 3 new BaseValuesPerPlayer(1100, 0, 655, 77)
	// 4 new BaseValuesPerPlayer(1300, 31, 655, 77);
	switch (difficultyId) {
		case 2:
			return BaseRewards{1300, 31, 655, 77};
		default:
			return BaseRewards{1100, 0, 655, 77};
	}
}

// Java ArenaOfGloryInstance.java:65-73
float ArenaOfGloryInstance::getRewardRate(int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0:
			return 0.55f;
		case 1:
			return 0.25f;
		case 2:
			return 0.15f;
		default:
			return 0.05f;
	}
}

// Java ArenaOfGloryInstance.java:75-112
void ArenaOfGloryInstance::setRewardItems(PvPArenaPlayerReward& reward, int32_t rank, int32_t difficultyId) {
	switch (rank) {
		case 0: {
			switch (difficultyId) {
				case 2: {
					reward.setRewardItem1(RewardItem::create(186000242, 3)); // Ceramium Medal
					reward.setRewardItem2(RewardItem::create(182213259, 1)); // Arena of Glory Ticket
					break;
				}
				default: {
					reward.setRewardItem1(RewardItem::create(186000147, 5)); // Mithril Medal
					reward.setRewardItem2(RewardItem::create(182213259, 1)); // Arena of Glory Ticket
					break;
				}
			}
			break;
		}
		case 1: {
			switch (difficultyId) {
				case 2:
					reward.setRewardItem1(RewardItem::create(186000242, 1)); // Ceramium Medal
					break;
				default: {
					reward.setRewardItem1(RewardItem::create(186000147, 1)); // Mithril Medal
					reward.setRewardItem1(RewardItem::create(186000096, 3)); // Platinum Medal
					break;
				}
			}
			break;
		}
		case 2: {
			switch (difficultyId) {
				case 2:
					reward.setRewardItem1(RewardItem::create(186000147, 2)); // Mithril Medal
					break;
				default:
					reward.setRewardItem1(RewardItem::create(186000096, 3)); // Platinum Medal
					break;
			}
			break;
		}
		default: {
			switch (difficultyId) {
				case 2:
					reward.setRewardItem1(RewardItem::create(162000124, 5)); // Superior Recovery Serum
					break;
				default:
					reward.setRewardItem1(RewardItem::create(162000077, 1)); // Fine Life Serum
					break;
			}
			break;
		}
	}
}

// Java ArenaOfGloryInstance.java:114-117
float ArenaOfGloryInstance::getConfigRate(runtime::Ptr<Player> player) {
	return model::gameobjects::player::get(*player, *RatesConfig::PVP_ARENA_GLORY_REWARD_RATES.get());
}

} // namespace aion::gameserver::handlers::instance::pvparenas
