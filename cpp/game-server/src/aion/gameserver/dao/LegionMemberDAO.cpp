#include "aion/gameserver/dao/LegionMemberDAO.h"

#include <string>
#include <string_view>

#include "aion/commons/database/DB.h"
#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/team/legion/LegionRank.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/LegionService.h"

namespace aion::gameserver::dao {

using commons::database::DatabaseFactory;
using commons::database::DB;
using commons::database::PreparedStatement;
using commons::database::SQLException;
using model::team::legion::LegionMember;
using model::team::legion::LegionRank;

namespace {

constexpr std::string_view INSERT_LEGIONMEMBER_QUERY = "INSERT INTO legion_members(`legion_id`, `player_id`, `rank`) VALUES (?, ?, ?)";
constexpr std::string_view UPDATE_LEGIONMEMBER_QUERY = "UPDATE legion_members SET nickname=?, `rank`=?, selfintro=?, challenge_score=? WHERE player_id=?";
constexpr std::string_view UPDATE_RANK_QUERY = "UPDATE legion_members SET `rank`=? WHERE player_id=?";
constexpr std::string_view SELECT_LEGIONMEMBER_QUERY = "SELECT * FROM legion_members WHERE player_id = ?";
constexpr std::string_view DELETE_LEGIONMEMBER_QUERY = "DELETE FROM legion_members WHERE player_id = ?";
constexpr std::string_view SELECT_LEGIONMEMBERS_QUERY = "SELECT player_id FROM legion_members WHERE legion_id = ?";

} // namespace

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.LegionMemberDAO");

bool LegionMemberDAO::isIdUsed(int32_t playerObjId) {
	std::unique_ptr<PreparedStatement> s = DB::prepareStatement("SELECT count(player_id) as cnt FROM legion_members WHERE ? = legion_members.player_id");
	if (!s) // Java: s.setInt on the null that DB.prepareStatement returned (the finally block's DB.close(null) does nothing)
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"s\" is null");
	try {
		s->setInt(1, playerObjId);
		auto rs = s->executeQuery();
		rs->next();
		bool used = rs->getInt("cnt") > 0;
		DB::close(s);
		return used;
	} catch (const SQLException& e) {
		DB::close(s);
		log.error("Can't check if name " + std::to_string(playerObjId) + ", is used, returning possitive result", e);
		return true;
	} catch (...) {
		DB::close(s);
		throw;
	}
}

bool LegionMemberDAO::saveNewLegionMember(model::team::legion::LegionMember& legionMember) {
	bool success = DB::insertUpdate(INSERT_LEGIONMEMBER_QUERY, [&](PreparedStatement& preparedStatement) {
		preparedStatement.setInt(1, legionMember.getLegion()->getLegionId());
		preparedStatement.setInt(2, legionMember.getObjectId());
		preparedStatement.setString(3, detail::enumName(legionMember.getRank()));
		preparedStatement.execute();
	});
	return success;
}

void LegionMemberDAO::storeLegionMember(model::team::legion::LegionMember& legionMember) {
	DB::insertUpdate(UPDATE_LEGIONMEMBER_QUERY, [&](PreparedStatement& stmt) {
		stmt.setString(1, legionMember.getNickname());
		stmt.setString(2, detail::enumName(legionMember.getRank()));
		stmt.setString(3, legionMember.getSelfIntro());
		stmt.setInt(4, legionMember.getChallengeScore());
		stmt.setInt(5, legionMember.getObjectId());
		stmt.execute();
	});
}

runtime::Ref<model::team::legion::LegionMember> LegionMemberDAO::loadLegionMember(int32_t playerObjId) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_LEGIONMEMBER_QUERY);
		stmt->setInt(1, playerObjId);
		auto resultSet = stmt->executeQuery();
		if (!resultSet->next())
			return nullptr;
		int32_t legionId = resultSet->getInt("legion_id");
		runtime::Ptr<model::team::legion::Legion> legion = services::LegionService::getInstance().getLegion(legionId);
		if (!legion) // disbanded by calling getLegion
			return nullptr;
		runtime::Ref<LegionMember> legionMember = LegionMember::create(playerObjId, *legion);
		legionMember->setRank(detail::enumValueOf<LegionRank>(resultSet->getString("rank"), "com.aionemu.gameserver.model.team.legion.LegionRank"));
		legionMember->setNickname(resultSet->getString("nickname"));
		legionMember->setSelfIntro(resultSet->getString("selfintro"));
		legionMember->setChallengeScore(resultSet->getInt("challenge_score"));
		return legionMember;
	} catch (const SQLException& e) {
		log.error("Could not load legion member " + std::to_string(playerObjId), e);
		return nullptr;
	}
}

std::vector<int32_t> LegionMemberDAO::loadLegionMembers(int32_t legionId) {
	std::vector<int32_t> legionMembers;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_LEGIONMEMBERS_QUERY);
		stmt->setInt(1, legionId);
		auto rs = stmt->executeQuery();
		while (rs->next())
			legionMembers.push_back(rs->getInt("player_id"));
	} catch (const SQLException&) {
		throw commons::utils::Exception("Could not load members of legion " + std::to_string(legionId), std::current_exception());
	}
	return legionMembers;
}

void LegionMemberDAO::deleteLegionMember(int32_t playerObjId) {
	std::unique_ptr<PreparedStatement> statement = DB::prepareStatement(DELETE_LEGIONMEMBER_QUERY);
	if (!statement) // Java: statement.setInt on the null that DB.prepareStatement returned
		throw runtime::NullPointerException("Cannot invoke \"java.sql.PreparedStatement.setInt(int, int)\" because \"statement\" is null");
	try {
		statement->setInt(1, playerObjId);
	} catch (const SQLException& e) {
		log.error("Some crap, can't set int parameter to PreparedStatement", e);
	}
	DB::executeUpdateAndClose(statement);
}

bool LegionMemberDAO::setRank(int32_t playerId, model::team::legion::LegionRank legionRank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_RANK_QUERY);
		stmt->setString(1, detail::enumName(legionRank));
		stmt->setInt(2, playerId);
		return stmt->executeUpdate() > 0;
	} catch (const SQLException& e) {
		log.error("Could not set rank of player {} to {}", playerId, detail::enumName(legionRank), e);
		return false;
	}
}

} // namespace aion::gameserver::dao
