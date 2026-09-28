#include "aion/gameserver/network/loginserver/clientpackets/CM_PTRANSFER_RESPONSE.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/transfers/PlayerTransfer.h"
#include "aion/gameserver/services/transfers/PlayerTransferService.h"

namespace aion::gameserver::network::loginserver::clientpackets {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
		new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.loginserver.clientpackets.CM_PTRANSFER_RESPONSE"));
	return *logger;
}

/** Java: byte[] db = readB(len), stored as the transfer's byte array */
runtime::Ref<runtime::Array<int8_t>> toByteArray(const std::vector<uint8_t>& bytes) {
	runtime::Ref<runtime::Array<int8_t>> array = runtime::Array<int8_t>::make(static_cast<int32_t>(bytes.size()));
	for (size_t i = 0; i < bytes.size(); i++)
		(*array)[static_cast<int32_t>(i)] = static_cast<int8_t>(bytes[i]);
	return array;
}

} // namespace

using services::transfers::PlayerTransfer;
using services::transfers::PlayerTransferService;

CM_PTRANSFER_RESPONSE::CM_PTRANSFER_RESPONSE(int32_t opCode) : LsClientPacket(opCode) {
}

void CM_PTRANSFER_RESPONSE::readImpl() {
	int32_t actionId = this->readD();
	switch (actionId) {
		case 20: // send info
		{
			int32_t targetAccount = readD();
			int32_t taskId = readD();
			std::string name = readS();
			std::string account = readS();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ref<PlayerTransfer> transfer = PlayerTransfer::create(taskId, targetAccount, account, name);
			transfer->setCommonData(db);
			PlayerTransferService::getInstance().putTransfer(taskId, *transfer);
		} break;
		case 24: // send items
		{
			int32_t taskId = readD();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ptr<PlayerTransfer> transfer = PlayerTransferService::getInstance().getTransfer(taskId);
			transfer->setItemsData(db);
		} break;
		case 25: // send data
		{
			int32_t taskId = readD();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ptr<PlayerTransfer> transfer = PlayerTransferService::getInstance().getTransfer(taskId);
			transfer->setData(db);
		} break;
		case 26: // send skill
		{
			int32_t taskId = readD();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ptr<PlayerTransfer> transfer = PlayerTransferService::getInstance().getTransfer(taskId);
			transfer->setSkillData(db);
		} break;
		case 27: // send recipe
		{
			int32_t taskId = readD();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ptr<PlayerTransfer> transfer = PlayerTransferService::getInstance().getTransfer(taskId);
			transfer->setRecipeData(db);
		} break;
		case 28: // send quest
		{
			int32_t taskId = readD();
			int32_t len = readD();
			runtime::Ref<runtime::Array<int8_t>> db = toByteArray(this->readB(len));
			runtime::Ptr<PlayerTransfer> transfer = PlayerTransferService::getInstance().getTransfer(taskId);
			transfer->setQuestData(db);
			PlayerTransferService::getInstance().cloneCharacter(taskId, *transfer);
		} break;
		case 21: // ok
		{
			int32_t taskId = readD();
			PlayerTransferService::getInstance().onOk(taskId);
		} break;
		case 22: // error
		{
			int32_t taskId = readD();
			std::string reason = readS();
			PlayerTransferService::getInstance().onError(taskId, reason);
		} break;
		case 23: {
			int8_t serverId = readC();
			if (configs::network::NetworkConfig::GAMESERVER_ID.load() != serverId) {
				// Java: throws and catches an Exception to print its stack trace
				commons::utils::Exception e("Requesting player transfer for server id " + std::to_string(serverId) + " but this is " +
					std::to_string(configs::network::NetworkConfig::GAMESERVER_ID.load()) + " omgshit!");
				log().error("", e);
			} else {
				int8_t targetServerId = readC();
				int32_t account = readD();
				int32_t targetAccount = readD();
				int32_t playerId = readD();
				int32_t taskId = readD();
				PlayerTransferService::getInstance().startTransfer(account, targetAccount, playerId, targetServerId, taskId);
			}
		} break;
	}
}

} // namespace aion::gameserver::network::loginserver::clientpackets
