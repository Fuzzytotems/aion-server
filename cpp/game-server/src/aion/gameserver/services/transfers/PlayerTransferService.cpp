#include "aion/gameserver/services/transfers/PlayerTransferService.h"

#include "aion/commons/logging/LoggerFactory.h"
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
	AION_UNPORTED();
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
