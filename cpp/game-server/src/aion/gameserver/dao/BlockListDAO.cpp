#include "aion/gameserver/dao/BlockListDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at BlockListDAO.java:34 (com.aionemu.gameserver.dao.BlockListDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at BlockListDAO.java:47 (com.aionemu.gameserver.dao.BlockListDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous ParamReadStH at BlockListDAO.java:61 (com.aionemu.gameserver.dao.BlockListDAO$3); argument 2 of select(); storage: sync
//   anonymous IUStH at BlockListDAO.java:86 (com.aionemu.gameserver.dao.BlockListDAO$4); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view LOAD_QUERY = "SELECT blocked_player, reason FROM blocks WHERE player=?";
constexpr std::string_view ADD_QUERY = "INSERT INTO blocks (player, blocked_player, reason) VALUES (?, ?, ?)";
constexpr std::string_view DEL_QUERY = "DELETE FROM blocks WHERE player=? AND blocked_player=?";
constexpr std::string_view SET_REASON_QUERY = "UPDATE blocks SET reason=? WHERE player=? AND blocked_player=?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BlockListDAO");

bool BlockListDAO::addBlockedUser(int32_t playerObjId, int32_t objIdToBlock, std::string_view reason) {
	AION_UNPORTED();
}

bool BlockListDAO::delBlockedUser(int32_t playerObjId, int32_t objIdToDelete) {
	AION_UNPORTED();
}

runtime::Ref<model::gameobjects::player::BlockList> BlockListDAO::load(int32_t playerObjId) {
	AION_UNPORTED();
}

bool BlockListDAO::setReason(int32_t playerObjId, int32_t blockedPlayerObjId, std::string_view reason) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
