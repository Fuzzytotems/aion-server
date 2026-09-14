#include "aion/gameserver/dao/HeadhuntingDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at HeadhuntingDAO.java:28 (com.aionemu.gameserver.dao.HeadhuntingDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at HeadhuntingDAO.java:51 (com.aionemu.gameserver.dao.HeadhuntingDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at HeadhuntingDAO.java:65 (com.aionemu.gameserver.dao.HeadhuntingDAO$3); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view SELECT_QUERY = "SELECT * FROM `headhunting`";
constexpr std::string_view UPDATE_QUERY = "REPLACE INTO `headhunting` (`hunter_id`, `accumulated_kills`, `last_update`) VALUES (?,?,?)";
constexpr std::string_view DELETE_QUERY = "DELETE FROM `headhunting`";

} // namespace

std::map<int32_t, runtime::Ref<model::event::Headhunter>> HeadhuntingDAO::loadHeadhunters() {
	AION_UNPORTED();
}

bool HeadhuntingDAO::clearTables() {
	AION_UNPORTED();
}

void HeadhuntingDAO::storeHeadhunter(int32_t hunterId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
