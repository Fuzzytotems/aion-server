#include "aion/loginserver/network/gameserver/serverpackets/SM_PTRANSFER_RESPONSE.h"

#include "aion/commons/utils/Exception.h"

namespace aion::loginserver::network::gameserver::serverpackets {

void SM_PTRANSFER_RESPONSE::writeImpl(GsConnection& con, commons::utils::ByteBuffer& buf) const {
	writeC(buf, 12);
	writeD(buf, getId(result));
	switch (result) {
		case PlayerTransferResultStatus::SEND_INFO:
			if (!request || !account)
				throw commons::utils::IllegalStateException("SM_PTRANSFER_RESPONSE SEND_INFO without request or account");
			writeD(buf, request->targetAccountId);
			writeD(buf, taskId);
			writeS(buf, request->name);
			writeS(buf, account->getName());
			writeD(buf, static_cast<int32_t>(request->db.size()));
			writeB(buf, request->db);
			break;
		case PlayerTransferResultStatus::OK:
			writeD(buf, taskId);
			break;
		case PlayerTransferResultStatus::ERROR_:
			writeD(buf, taskId);
			writeS(buf, reason);
			break;
		case PlayerTransferResultStatus::PERFORM_ACTION:
			writeC(buf, task.sourceServerId);
			writeC(buf, task.targetServerId);
			writeD(buf, task.sourceAccountId);
			writeD(buf, task.targetAccountId);
			writeD(buf, task.playerId);
			writeD(buf, task.id);
			break;
	}
}

} // namespace aion::loginserver::network::gameserver::serverpackets
