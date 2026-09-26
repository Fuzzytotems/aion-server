#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "aion/loginserver/network/gameserver/GsServerPacket.h"
#include "aion/loginserver/service/ptransfer/PlayerTransferRequest.h"
#include "aion/loginserver/service/ptransfer/PlayerTransferResultStatus.h"
#include "aion/loginserver/service/ptransfer/PlayerTransferTask.h"

namespace aion::loginserver::network::gameserver::serverpackets {

/**
 * Player transfer messages to the source or target game server. Which fields are written depends on the result status.
 * <p>
 * Java: com.aionemu.loginserver.network.gameserver.serverpackets.SM_PTRANSFER_RESPONSE
 *
 * @author KID
 */
class SM_PTRANSFER_RESPONSE : public GsServerPacket {
public:
	using PlayerTransferRequest = service::ptransfer::PlayerTransferRequest;
	using PlayerTransferResultStatus = service::ptransfer::PlayerTransferResultStatus;
	using PlayerTransferTask = service::ptransfer::PlayerTransferTask;

	SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus result, int32_t taskId) : result(result), taskId(taskId) {}

	/** @param request shared with PlayerTransferService, not modified after it was sent */
	SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus result, std::shared_ptr<const PlayerTransferRequest> request)
		: result(result), account(request->targetAccount), taskId(request->taskId), request(std::move(request)) {}

	SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus result, int32_t taskId, std::string reason) : result(result), taskId(taskId), reason(std::move(reason)) {}

	/** @param task copied */
	SM_PTRANSFER_RESPONSE(PlayerTransferResultStatus result, const PlayerTransferTask& task) : result(result), task(task) {}

protected:
	void writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const override;

private:
	const PlayerTransferResultStatus result;
	const std::shared_ptr<model::Account> account;
	const int32_t taskId = 0;
	const std::shared_ptr<const PlayerTransferRequest> request;
	const std::string reason;
	const PlayerTransferTask task;
};

} // namespace aion::loginserver::network::gameserver::serverpackets
