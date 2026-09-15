#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"

#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.h"
#include "aion/gameserver/model/event/ArcadeProgress.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeLevel.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewardItem.h"
#include "aion/gameserver/model/templates/event/upgradearcade/ArcadeRewards.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"

namespace aion::gameserver::network::aion::serverpackets {

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE()
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(2) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(bool showIconValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(0), showIcon(showIconValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue, int32_t sessionIdValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(1), progress(progressValue), sessionId(sessionIdValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(bool successValue, model::event::ArcadeProgress& progressValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(3), progress(progressValue), success(successValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(4), progress(progressValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progressValue, bool resumeAllowed)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(5), progress(progressValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t itemId, int64_t count)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(6), rewardItemId(itemId), rewardItemCount(count) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t frenzyDurationSecondsValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(7), frenzyDurationSeconds(frenzyDurationSecondsValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(int32_t actionValue, bool disableWindowValue)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(actionValue), disableWindow(disableWindowValue) {
}

SM_UPGRADE_ARCADE::SM_UPGRADE_ARCADE(const std::vector<const model::templates::event::upgradearcade::ArcadeRewards*>& rewards)
	: AionServerPacket(opcodeOf<SM_UPGRADE_ARCADE>), action(10), arcadeRewards(rewards) {
}

SM_UPGRADE_ARCADE::~SM_UPGRADE_ARCADE() = default;

void SM_UPGRADE_ARCADE::writeImpl(AionConnection* con) {
	writeC(action);
	switch (action) {
		case 0: // show icon
			writeD(showIcon ? 1 : 0);
			break;
		case 1: { // show start upgrade arcade info
			const dataholders::UpgradeArcadeData& upgradeArcadeData = *dataholders::DataManager::UPGRADE_ARCADE_DATA;
			writeD(sessionId); // SessionId
			writeD(progress->getFrenzyPoints()); // frenzy meter
			for (const model::templates::event::upgradearcade::ArcadeRewards& arcadeReward : upgradeArcadeData.getRewards())
				writeD(arcadeReward.getMinLevel());
			writeD(upgradeArcadeData.getMaxUpgradeLevel()->getLevel());
			writeC(1);
			writeC(static_cast<int32_t>(upgradeArcadeData.getUpgradeLevels().size()) * 2);
			for (const model::templates::event::upgradearcade::ArcadeLevel& arcadeLevel : upgradeArcadeData.getUpgradeLevels())
				writeS(arcadeLevel.getIcon());
			break;
		}
		case 2: // open upgrade arcade
			writeC(1); // unk
			break;
		case 3: // upgrade start
			writeC(success ? 1 : 0); // 1 success - 0 fail
			writeD(progress->getFrenzyPoints());
			break;
		case 4: // update success
			writeD(progress->getCurrentLevel()); // upgradeLevel
			break;
		case 5: // upgrade fail
			writeD(progress->getCurrentLevel()); // upgradeLevel
			writeC(progress->getResumeLevel() > 0 ? 1 : 0); // canResume? 1 yes - 0 no
			writeQ(configs::main::EventsConfig::ARCADE_RESUME_TOKEN.load()); // needed Arcade Token
			break;
		case 6: // show reward item
			writeD(rewardItemId);
			writeQ(rewardItemCount);
			break;
		case 7: // frenzy time
			writeD(frenzyDurationSeconds);
			break;
		case 8: // disable window
			writeC(disableWindow ? 1 : 0); // msg when true: you don't have enough tokens
			break;
		case 10: // show reward list
			for (const model::templates::event::upgradearcade::ArcadeRewards* arcadetab : arcadeRewards)
				writeC(static_cast<int32_t>(arcadetab->getArcadeRewardItems().size()));
			for (const model::templates::event::upgradearcade::ArcadeRewards* arcadetab : arcadeRewards) {
				for (const model::templates::event::upgradearcade::ArcadeRewardItem& arcadetabitem : arcadetab->getArcadeRewardItems()) {
					writeD(arcadetabitem.getItemId());
					writeQ(arcadetabitem.getNormalCount());
					writeQ(arcadetabitem.getFrenzyCount());
				}
			}
			break;
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
