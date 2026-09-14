#include "aion/gameserver/dao/LegionMemberDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous IUStH at LegionMemberDAO.java:54 (com.aionemu.gameserver.dao.LegionMemberDAO$1); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at LegionMemberDAO.java:68 (com.aionemu.gameserver.dao.LegionMemberDAO$2); argument 2 of insertUpdate(); storage: sync

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
constexpr std::string_view INSERT_LEGIONMEMBER_QUERY = "INSERT INTO legion_members(`legion_id`, `player_id`, `rank`) VALUES (?, ?, ?)";
constexpr std::string_view UPDATE_LEGIONMEMBER_QUERY = "UPDATE legion_members SET nickname=?, `rank`=?, selfintro=?, challenge_score=? WHERE player_id=?";
constexpr std::string_view UPDATE_RANK_QUERY = "UPDATE legion_members SET `rank`=? WHERE player_id=?";
constexpr std::string_view SELECT_LEGIONMEMBER_QUERY = "SELECT * FROM legion_members WHERE player_id = ?";
constexpr std::string_view DELETE_LEGIONMEMBER_QUERY = "DELETE FROM legion_members WHERE player_id = ?";
constexpr std::string_view SELECT_LEGIONMEMBERS_QUERY = "SELECT player_id FROM legion_members WHERE legion_id = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionMemberDAO");

bool LegionMemberDAO::isIdUsed(int32_t playerObjId) {
	AION_UNPORTED();
}

bool LegionMemberDAO::saveNewLegionMember(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

void LegionMemberDAO::storeLegionMember(model::team::legion::LegionMember& legionMember) {
	AION_UNPORTED();
}

runtime::Ref<model::team::legion::LegionMember> LegionMemberDAO::loadLegionMember(int32_t playerObjId) {
	AION_UNPORTED();
}

std::vector<int32_t> LegionMemberDAO::loadLegionMembers(int32_t legionId) {
	AION_UNPORTED();
}

void LegionMemberDAO::deleteLegionMember(int32_t playerObjId) {
	AION_UNPORTED();
}

bool LegionMemberDAO::setRank(int32_t playerId, model::team::legion::LegionRank legionRank) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
