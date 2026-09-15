#include "aion/gameserver/dao/BlockListDAO.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::dao {

using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::ResultSet;
using model::gameobjects::player::BlockedPlayer;

namespace {

constexpr std::string_view LOAD_QUERY = "SELECT blocked_player, reason FROM blocks WHERE player=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO blocks (player, blocked_player, reason) VALUES (?, ?, ?)";
constexpr std::string_view DEL_QUERY = "DELETE FROM blocks WHERE player=? AND blocked_player=?";
constexpr std::string_view SET_REASON_QUERY = "UPDATE blocks SET reason=? WHERE player=? AND blocked_player=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BlockListDAO");

bool BlockListDAO::addBlockedUser(int32_t playerObjId, int32_t objIdToBlock, std::string_view reason) {
	return DB::insertUpdate(ADD_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerObjId);
		stmt.setInt(2, objIdToBlock);
		stmt.setString(3, reason);
		stmt.execute();
	});
}

bool BlockListDAO::delBlockedUser(int32_t playerObjId, int32_t objIdToDelete) {
	return DB::insertUpdate(DEL_QUERY, [&](PreparedStatement& stmt) {
		stmt.setInt(1, playerObjId);
		stmt.setInt(2, objIdToDelete);
		stmt.execute();
	});
}

runtime::Ref<model::gameobjects::player::BlockList> BlockListDAO::load(int32_t playerObjId) {
	// Java: Map<Integer, BlockedPlayer> list = new HashMap<>(); the Refs keep the entries alive until the BlockList holds them
	std::vector<runtime::Ref<BlockedPlayer>> loaded;
	std::unordered_map<int32_t, runtime::Ptr<BlockedPlayer>> list;
	DB::select(
		LOAD_QUERY, [&](PreparedStatement& stmt) { stmt.setInt(1, playerObjId); },
		[&](ResultSet& rset) {
			while (rset.next()) {
				int32_t blockedOid = rset.getInt("blocked_player");
				std::optional<std::string> name = services::player::PlayerService::getPlayerName(blockedOid);
				if (!name) {
					log.error("Attempt to load block list for player " + std::to_string(playerObjId) +
						" tried to load a player which does not exist: " + std::to_string(blockedOid));
				} else {
					runtime::Ref<BlockedPlayer> blocked = BlockedPlayer::create(blockedOid, *name, rset.getString("reason"));
					list.insert_or_assign(blockedOid, runtime::Ptr<BlockedPlayer>(blocked));
					loaded.push_back(std::move(blocked));
				}
			}
		});
	return model::gameobjects::player::BlockList::create(list);
}

bool BlockListDAO::setReason(int32_t playerObjId, int32_t blockedPlayerObjId, std::string_view reason) {
	return DB::insertUpdate(SET_REASON_QUERY, [&](PreparedStatement& stmt) {
		stmt.setString(1, reason);
		stmt.setInt(2, playerObjId);
		stmt.setInt(3, blockedPlayerObjId);
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
