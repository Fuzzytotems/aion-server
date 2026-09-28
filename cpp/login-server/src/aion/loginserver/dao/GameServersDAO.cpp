#include "aion/loginserver/dao/GameServersDAO.h"

#include "aion/commons/database/DB.h"

namespace aion::loginserver::dao::GameServersDAO {

using commons::database::DB;
using commons::database::ResultSet;

bool forEachGameServer(const std::function<void(int8_t id, std::string ipMask, std::string password)>& consumer) {
	return DB::select("SELECT * FROM gameservers", [&](ResultSet& resultSet) {
		while (resultSet.next()) {
			int8_t id = resultSet.getByte("id");
			std::string ipMask = resultSet.getString("mask");
			std::string password = resultSet.getString("password");
			consumer(id, std::move(ipMask), std::move(password));
		}
	});
}

} // namespace aion::loginserver::dao::GameServersDAO
