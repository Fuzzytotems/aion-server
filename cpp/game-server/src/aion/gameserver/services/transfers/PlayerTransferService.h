#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/services/transfers/fwd.h"

namespace aion::gameserver::services::transfers {

/**
 * @author KID
 */
class PlayerTransferService : public runtime::Immortal {
private:
	runtime::LinkedHashMap<int32_t, runtime::Ref<TransferablePlayer>> transfers{AION_LOCK_CLASS(PlayerTransferService::transfers)};
	runtime::HashMap<int32_t, runtime::Ref<PlayerTransfer>> playerTransfers{AION_LOCK_CLASS(PlayerTransferService::playerTransfers)};
	runtime::ArrayList<int32_t> rsList{AION_LOCK_CLASS(PlayerTransferService::rsList)}; // Java: = new ArrayList<>()
	/** Java public constructor; C++ Immortal singleton: only getInstance() constructs it (hub-headers.md §11.2) */
	PlayerTransferService();
	~PlayerTransferService();

public:
	static PlayerTransferService& getInstance(); // Java singleton
	void startTransfer(int32_t accountId, int32_t targetAccountId, int32_t playerId, int8_t targetServerId, int32_t taskId);
	/** sent from login to target server with character information from source server */
	void cloneCharacter(int32_t taskId, PlayerTransfer& transfer);
	/** from login server to source, after response from target server */
	void onOk(int32_t taskId);
	/** from login server to source, after response from target server */
	void onError(int32_t taskId, std::string_view reason);
	void putTransfer(int32_t taskId, PlayerTransfer& playerTransfer);
	runtime::Ptr<PlayerTransfer> getTransfer(int32_t taskId);
};

} // namespace aion::gameserver::services::transfers
