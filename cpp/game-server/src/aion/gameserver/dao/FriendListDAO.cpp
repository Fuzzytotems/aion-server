#include "aion/gameserver/dao/FriendListDAO.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using model::gameobjects::player::Friend;
using model::gameobjects::player::FriendList;

namespace {

constexpr std::string_view LOAD_QUERY = "SELECT * FROM `friends` WHERE `player`=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO `friends` (`player`,`friend`) VALUES (?, ?)";
constexpr std::string_view DEL_QUERY = "DELETE FROM friends WHERE player = ? AND friend = ?";
constexpr std::string_view SET_MEMO_QUERY = "UPDATE friends SET memo=? WHERE player=? AND friend=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.FriendListDAO");

namespace {

/**
 * The Java body of load. FriendList is an OwnedPart (Player::setFriendList takes a std::unique_ptr), so the result is a std::unique_ptr: a Ref
 * cannot hold a part (header request dao-1).
 */
std::unique_ptr<FriendList> loadFriendListPart(model::gameobjects::player::Player& player) {
	std::vector<runtime::Ref<Friend>> friends;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(LOAD_QUERY);
		stmt->setInt(1, player.getObjectId());
		auto rset = stmt->executeQuery();
		while (rset->next()) {
			int32_t objId = rset->getInt("friend");
			runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = services::player::PlayerService::getOrLoadPlayerCommonData(objId);
			if (pcd) {
				friends.push_back(Friend::create(*pcd, rset->getString("memo")));
			}
		}
	} catch (const std::exception& e) {
		log.error("Could not restore FriendList data for player: " + std::to_string(player.getObjectId()) + " from DB: " + e.what(), e);
	}
	std::vector<runtime::Ptr<Friend>> friendPtrs(friends.begin(), friends.end());
	return std::make_unique<FriendList>(player, friendPtrs);
}

} // namespace

std::unique_ptr<model::gameobjects::player::FriendList> FriendListDAO::load(model::gameobjects::player::Player& player) {
	return loadFriendListPart(player);
}

bool FriendListDAO::addFriends(model::gameobjects::player::Player& player, model::gameobjects::player::Player& friend_) {
	return DB::insertUpdate(ADD_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, player.getObjectId());
		ps.setInt(2, friend_.getObjectId());
		ps.addBatch();

		ps.setInt(1, friend_.getObjectId());
		ps.setInt(2, player.getObjectId());
		ps.addBatch();

		ps.executeBatch();
	});
}

bool FriendListDAO::delFriends(int32_t playerOid, int32_t friendOid) {
	return DB::insertUpdate(DEL_QUERY, [&](PreparedStatement& ps) {
		ps.setInt(1, playerOid);
		ps.setInt(2, friendOid);
		ps.addBatch();

		ps.setInt(1, friendOid);
		ps.setInt(2, playerOid);
		ps.addBatch();

		ps.executeBatch();
	});
}

bool FriendListDAO::setFriendMemo(int32_t playerOid, int32_t friendOid, std::string_view memo) {
	return DB::insertUpdate(SET_MEMO_QUERY, [&](PreparedStatement& stmt) {
		stmt.setString(1, memo);
		stmt.setInt(2, playerOid);
		stmt.setInt(3, friendOid);
		stmt.execute();
	});
}

} // namespace aion::gameserver::dao
