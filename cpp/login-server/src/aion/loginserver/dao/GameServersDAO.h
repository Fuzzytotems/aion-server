#pragma once

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

/**
 * Access to the gameservers table (registered game servers). Thread safe.
 * <p>
 * Java: com.aionemu.loginserver.dao.GameServersDAO
 *
 * @author -Nemesiss-
 */
namespace aion::loginserver::dao::GameServersDAO {

/**
 * C++ helper for getAllGameServers: reads all rows and calls the consumer with id, mask and password of each game server. Errors are logged by
 * DB ("Error executing select query ..."); the rows read before the error have been passed to the consumer.
 *
 * @return true if the query ran successfully
 */
bool forEachGameServer(const std::function<void(int8_t id, std::string ipMask, std::string password)>& consumer);

/**
 * Loads all registered game servers.
 * <p>
 * Java: getAllGameServers() returns Map&lt;Byte, GameServerInfo&gt;. GameServerInfo belongs to the login server core (not to the data layer),
 * so the class is a template parameter: <code>GameServersDAO::getAllGameServers&lt;GameServerInfo&gt;()</code>. It must be constructible from
 * (int8_t id, std::string ipMask, std::string password). The infos are held by std::shared_ptr because game server connections refer to them.
 *
 * @return id -> game server info; on errors the servers read before the error
 */
template <typename GameServerInfo>
	requires std::constructible_from<GameServerInfo, int8_t, std::string, std::string>
std::unordered_map<int8_t, std::shared_ptr<GameServerInfo>> getAllGameServers() {
	std::unordered_map<int8_t, std::shared_ptr<GameServerInfo>> result;
	forEachGameServer([&](int8_t id, std::string ipMask, std::string password) {
		result.insert_or_assign(id, std::make_shared<GameServerInfo>(id, std::move(ipMask), std::move(password)));
	});
	return result;
}

} // namespace aion::loginserver::dao::GameServersDAO
