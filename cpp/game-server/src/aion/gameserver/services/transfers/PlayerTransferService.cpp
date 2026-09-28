#include "aion/gameserver/services/transfers/PlayerTransferService.h"

#include <memory>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/transfers/PlayerTransfer.h"
#include "aion/gameserver/services/transfers/TransferablePlayer.h"

namespace aion::gameserver::services::transfers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.transfers.PlayerTransferService");
static const auto textLog = commons::logging::LoggerFactory::getLogger("PLAYERTRANSFER");

PlayerTransferService& PlayerTransferService::getInstance() {
	static PlayerTransferService instance; // Java SingletonHolder
	return instance;
}

PlayerTransferService::PlayerTransferService() {
	std::shared_ptr<const std::string> removeSkillList = configs::main::PlayerTransferConfig::REMOVE_SKILL_LIST.get();
	if (*removeSkillList != "*") {
		for (const std::string& skillId : commons::utils::StringUtils::splitJava(*removeSkillList, ","))
			rsList.add(commons::utils::parseInt(skillId)); // Java: Integer.parseInt (NumberFormatException for a malformed id)
	}
	log.info("PlayerTransferService loaded. With " + std::to_string(rsList.size()) + " restricted skills.");
}

PlayerTransferService::~PlayerTransferService() = default;

void PlayerTransferService::startTransfer(int32_t accountId, int32_t targetAccountId, int32_t playerId, int8_t targetServerId, int32_t taskId) {
	AION_UNPORTED();
}

void PlayerTransferService::cloneCharacter(int32_t taskId, PlayerTransfer& transfer) {
	AION_UNPORTED();
}

void PlayerTransferService::onOk(int32_t taskId) {
	AION_UNPORTED();
}

void PlayerTransferService::onError(int32_t taskId, std::string_view reason) {
	AION_UNPORTED();
}

void PlayerTransferService::putTransfer(int32_t taskId, PlayerTransfer& playerTransfer) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerTransfer> PlayerTransferService::getTransfer(int32_t taskId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::transfers
