#include "aion/chatserver/dao/ChatLogDAO.h"

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::chatserver::dao::ChatLogDAO {

namespace {

constexpr std::string_view INSERT_QUERY = "INSERT INTO `chatlog` (`sender`, `message`, `type`) VALUES (?, ?, ?)";

} // namespace

void save(std::string_view sender, std::string_view message, std::string_view type) {
	try {
		auto con = commons::database::DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setString(1, sender);
		stmt->setString(2, message);
		stmt->setString(3, type);
		stmt->execute();
	} catch (const std::exception& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.chatserver.dao.ChatLogDAO").error("Cannot insert chat message", e);
	}
}

} // namespace aion::chatserver::dao::ChatLogDAO
