#include "aion/loginserver/network/gameserver/clientpackets/CM_PTRANSFER_CONTROL.h"

#include "aion/loginserver/service/PlayerTransferService.h"

namespace aion::loginserver::network::gameserver::clientpackets {

using service::PlayerTransferService;

void CM_PTRANSFER_CONTROL::readImpl() {
	actionId = readC();
	switch (actionId) {
		case 1: // request transfer
		{
			taskId = readD();
			text = readS(); // name
			int32_t bytes = getRemainingBytes();
			db = readB(bytes);
		} break;
		case 2: // ERROR
		{
			taskId = readD();
			text = readS(); // reason
		} break;
		case 3: // ok
		{
			taskId = readD();
		} break;
		case 4: // Task stop
		{
			taskId = readD();
			text = readS(); // reason
		}
	}
}

void CM_PTRANSFER_CONTROL::runImpl() {
	switch (actionId) {
		case 1:
			PlayerTransferService::getInstance().requestTransfer(taskId, text, std::move(db));
			break;
		case 2:
			PlayerTransferService::getInstance().onError(taskId, text);
			break;
		case 3:
			PlayerTransferService::getInstance().onOk(taskId);
			break;
		case 4:
			PlayerTransferService::getInstance().onTaskStop(taskId, text);
			break;
	}
}

} // namespace aion::loginserver::network::gameserver::clientpackets
