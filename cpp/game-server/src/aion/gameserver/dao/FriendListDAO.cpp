#include "aion/gameserver/dao/FriendListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at FriendListDAO.java:55 (com.aionemu.gameserver.dao.FriendListDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at FriendListDAO.java:74 (com.aionemu.gameserver.dao.FriendListDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at FriendListDAO.java:92 (com.aionemu.gameserver.dao.FriendListDAO$3); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view LOAD_QUERY = "SELECT * FROM `friends` WHERE `player`=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO `friends` (`player`,`friend`) VALUES (?, ?)";
constexpr std::string_view DEL_QUERY = "DELETE FROM friends WHERE player = ? AND friend = ?";
constexpr std::string_view SET_MEMO_QUERY = "UPDATE friends SET memo=? WHERE player=? AND friend=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.FriendListDAO");

runtime::Ref<model::gameobjects::player::FriendList> FriendListDAO::load(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool FriendListDAO::addFriends(model::gameobjects::player::Player& player, model::gameobjects::player::Player& friend_) {
	AION_UNPORTED();
}

bool FriendListDAO::delFriends(int32_t playerOid, int32_t friendOid) {
	AION_UNPORTED();
}

bool FriendListDAO::setFriendMemo(int32_t playerOid, int32_t friendOid, std::string_view memo) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
