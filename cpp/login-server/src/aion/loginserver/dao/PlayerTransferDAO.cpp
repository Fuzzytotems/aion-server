#include "aion/loginserver/dao/PlayerTransferDAO.h"

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::loginserver::dao::PlayerTransferDAO {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;

void forEachNewTask(const std::function<void(const NewTask& task)>& consumer) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto st = con->prepareStatement("SELECT * FROM player_transfers WHERE `status` = ?");
		st->setInt(1, 0);
		auto rs = st->executeQuery();
		while (rs->next()) {
			NewTask task;
			task.id = rs->getInt("id");
			task.sourceServerId = static_cast<int8_t>(rs->getShort("source_server"));
			task.targetServerId = static_cast<int8_t>(rs->getShort("target_server"));
			task.sourceAccountId = rs->getInt("source_account_id");
			task.targetAccountId = rs->getInt("target_account_id");
			task.playerId = rs->getInt("player_id");
			consumer(task);
		}
	} catch (const std::exception& e) {
		commons::logging::LoggerFactory::getLogger("com.aionemu.loginserver.dao.PlayerTransferDAO").error("Can't select getNew: ", e);
	}
}

bool update(int32_t id, int8_t status, std::optional<std::string_view> comment) {
	std::string_view table;
	switch (status) {
		case STATUS_ACTIVE:
			table = ", time_performed=NOW()";
			break;
		case STATUS_DONE:
		case STATUS_ERROR:
			table = ", time_done=NOW()";
			break;
		default:
			break;
	}
	return DB::insertUpdate("UPDATE player_transfers SET status=?, comment=?" + std::string(table) + " WHERE id=?", [&](PreparedStatement& preparedStatement) {
		preparedStatement.setByte(1, status);
		preparedStatement.setString(2, comment);
		preparedStatement.setInt(3, id);
		preparedStatement.execute();
	});
}

} // namespace aion::loginserver::dao::PlayerTransferDAO
