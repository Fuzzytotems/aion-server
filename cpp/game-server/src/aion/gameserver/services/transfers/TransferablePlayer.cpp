#include "aion/gameserver/services/transfers/TransferablePlayer.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::services::transfers {

TransferablePlayer::TransferablePlayer(int32_t value, int32_t accountIdValue, int32_t targetAccountIdValue)
	: playerId(value), accountId(accountIdValue), targetAccountId(targetAccountIdValue) {
}

runtime::Ref<TransferablePlayer> TransferablePlayer::create(int32_t value, int32_t accountIdValue, int32_t targetAccountIdValue) {
	return runtime::makeRef<TransferablePlayer>(value, accountIdValue, targetAccountIdValue);
}

TransferablePlayer::~TransferablePlayer() = default;

} // namespace aion::gameserver::services::transfers
