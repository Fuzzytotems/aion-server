#include "aion/loginserver/network/aion/AionClientPacket.h"

#include <string>
#include <utility>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/loginserver/model/Account.h"

namespace aion::loginserver::network::aion {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger = new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.network.aion.AionClientPacket"));
	return *logger;
}

} // namespace

AionClientPacket::AionClientPacket(commons::utils::ByteBuffer buf, std::shared_ptr<LoginConnection> client, int32_t opcode)
	: BaseClientPacket(std::move(buf), opcode) {
	setConnection(std::move(client));
}

void AionClientPacket::run() {
	try {
		runImpl();
	} catch (...) {
		try {
			std::string name;
			std::shared_ptr<model::Account> account = getConnection()->getAccount();
			if (account)
				name = account->getName();
			else
				name = getConnection()->getIP();

			log().errorCurrentException("error handling client (" + name + ") message " + toString());
		} catch (...) {
			// logging must not throw out of the packet processor
		}
	}
}

void AionClientPacket::sendPacket(std::shared_ptr<AionServerPacket> msg) const {
	getConnection()->sendPacket(std::move(msg));
}

} // namespace aion::loginserver::network::aion
