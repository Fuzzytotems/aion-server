#include "aion/gameserver/dao/AbyssRankDAO.h"

#include <string>
#include <unordered_map>

#include "aion/commons/database/DatabaseFactory.h"
#include "aion/commons/database/PreparedStatement.h"
#include "aion/commons/database/ResultSet.h"
#include "aion/commons/database/SQLException.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/RankingConfig.h"
#include "aion/gameserver/dao/detail/DaoSupport.h"
#include "aion/gameserver/dao/detail/JavaHash.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/model/Gender.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/detail/ObjectsData.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/utils/stats/AbyssRankEnum.h"

namespace aion::gameserver::dao {

namespace {

constexpr std::string_view SELECT_QUERY = "SELECT daily_ap, weekly_ap, ap, daily_gp, weekly_gp, gp, `rank`, daily_kill, weekly_kill, all_kill, max_rank, last_kill, last_ap, last_gp, last_update FROM abyss_rank WHERE player_id = ?";
constexpr std::string_view INSERT_QUERY = "INSERT INTO abyss_rank (player_id, daily_ap, weekly_ap, ap, `rank`, daily_kill, weekly_kill, all_kill, max_rank, last_kill, last_ap, last_update, daily_gp, weekly_gp, gp, last_gp) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
constexpr std::string_view UPDATE_QUERY = "UPDATE abyss_rank SET  daily_ap = ?, weekly_ap = ?, ap = ?, `rank` = ?, daily_kill = ?, weekly_kill = ?, all_kill = ?, max_rank = ?, last_kill = ?, last_ap = ?, last_update = ?, daily_gp = ?, weekly_gp = ?, gp = ?, last_gp = ? WHERE player_id = ?";
constexpr std::string_view DECREASE_GP_DAILY = "UPDATE abyss_rank SET gp = GREATEST(gp - ?, 0) WHERE `rank` = ?";
constexpr std::string_view INCREASE_GP_QUERY = "UPDATE abyss_rank SET gp = GREATEST(gp + ?, 0) WHERE player_id = ?";
constexpr std::string_view INCREASE_GP_QUERY_WITH_STATS = "UPDATE abyss_rank SET gp = gp + ?, daily_gp = daily_gp + ?, weekly_gp = weekly_gp + ? WHERE player_id = ?";
constexpr std::string_view UPDATE_RANK = "UPDATE abyss_rank SET `rank` = ? WHERE player_id = ?";
constexpr std::string_view SELECT_RANKING_LIST_PLAYERS = "SELECT a.rank_pos, a.old_rank_pos, p.id, p.name, p.race, p.exp, a.rank, a.ap, a.gp, p.title_id, p.player_class, p.gender, l.name FROM abyss_rank a JOIN players p ON a.player_id = p.id LEFT JOIN legion_members lm ON lm.player_id = p.id LEFT JOIN legions l ON l.id = lm.legion_id WHERE a.rank_pos > 0";
constexpr std::string_view SELECT_RANKING_LIST_LEGIONS = "SELECT l.rank_pos, l.old_rank_pos, l.id, l.name, p.race, l.level, l.contribution_points FROM legions l, legion_members lm, players p WHERE lm.rank = 'BRIGADE_GENERAL' AND lm.player_id = p.id AND lm.legion_id = l.id AND l.rank_pos > 0";
constexpr std::string_view SELECT_RANKING_LIST_PLAYERS_GP = "SELECT a.rank_pos, a.player_id, a.gp FROM abyss_rank a, players p WHERE a.player_id = p.id AND p.race = ? AND a.rank_pos > 0 ORDER by a.rank_pos";
constexpr std::string_view SELECT_UNRANKED_PLAYERS_AP = "SELECT a.player_id, a.ap FROM abyss_rank a, players p WHERE a.player_id = p.id AND p.race = ? AND a.rank_pos = 0 AND a.rank >= ?";
constexpr std::string_view SELECT_LEGION_COUNT = "SELECT COUNT(player_id) as players FROM legion_members WHERE legion_id = ?";
constexpr std::string_view RESET_RANKING_LIST_PLAYERS = "UPDATE abyss_rank a SET a.old_rank_pos = a.rank_pos, a.rank_pos = 0 WHERE a.rank_pos > 0";
constexpr std::string_view RESET_RANKING_LIST_LEGIONS = "UPDATE legions l SET l.old_rank_pos = l.rank_pos, l.rank_pos = 0 WHERE l.rank_pos > 0";
constexpr std::string_view UPDATE_RANKING_LIST_PLAYERS_POSITIONS = "UPDATE abyss_rank SET rank_pos = @a:=@a+1 WHERE gp > 0 AND player_id IN (SELECT id FROM players WHERE race = ? AND (@minLastOnline IS NULL OR last_online >= @minLastOnline)) ORDER BY gp DESC LIMIT ?";
constexpr std::string_view UPDATE_RANKING_LIST_LEGIONS_POSITIONS = "UPDATE legions SET rank_pos = @a:=@a+1 WHERE id IN (SELECT legion_id FROM legion_members lm, players WHERE `rank` = 'BRIGADE_GENERAL' AND players.id = lm.player_id and players.race = ?) ORDER BY contribution_points DESC LIMIT ?";

} // namespace

using commons::database::DatabaseFactory;
using commons::database::SQLException;
using model::gameobjects::Persistable;
using model::gameobjects::player::AbyssRank;
using utils::stats::AbyssRankEnum;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.AbyssRankDAO");

namespace {

/** Java AbyssRankEnum.getGpLossPerDay(): RankingConfig.TOP_RANKING_GP_LOSS.getOrDefault(this, 0) (the config map uses the placeholder enum) */
int32_t getGpLossPerDay(AbyssRankEnum rank) {
	auto gpLoss = configs::main::RankingConfig::TOP_RANKING_GP_LOSS.get();
	if (!gpLoss)
		return 0;
	auto it = gpLoss->find(static_cast<configs::detail::AbyssRankEnum>(static_cast<int32_t>(rank)));
	return it == gpLoss->end() ? 0 : it->second;
}

/** Java record hash of an enum component: its identity hash differs between JVM runs, the port uses the ordinal */
int32_t enumHash(auto value) {
	return static_cast<int32_t>(value);
}

} // namespace

AbyssRankDAO::RankingListPlayerGp::RankingListPlayerGp(int32_t position, int32_t playerId, int32_t gp)
	: position_(position), playerId_(playerId), gp_(gp) {
}

bool AbyssRankDAO::RankingListPlayerGp::equals(const RankingListPlayerGp& obj) const {
	return position_ == obj.position_ && playerId_ == obj.playerId_ && gp_ == obj.gp_;
}

int32_t AbyssRankDAO::RankingListPlayerGp::hashCode() const {
	int32_t result = 0;
	result = static_cast<int32_t>(static_cast<uint32_t>(result) * 31u + static_cast<uint32_t>(static_cast<int32_t>(position_)));
	result = static_cast<int32_t>(static_cast<uint32_t>(result) * 31u + static_cast<uint32_t>(static_cast<int32_t>(playerId_)));
	result = static_cast<int32_t>(static_cast<uint32_t>(result) * 31u + static_cast<uint32_t>(static_cast<int32_t>(gp_)));
	return result;
}

AbyssRankDAO::RankingListPlayer::RankingListPlayer(int32_t position, int32_t oldPosition, int32_t id, std::string_view name, model::Race race,
	int32_t level, int32_t abyssRank, int32_t ap, int32_t gp, int32_t title, model::PlayerClass playerClass, model::Gender gender,
	std::string_view legionName)
	: position_(position), oldPosition_(oldPosition), id_(id), name_(name), race_(race), level_(level), abyssRank_(abyssRank), ap_(ap), gp_(gp),
		title_(title), playerClass_(playerClass), gender_(gender), legionName_(legionName) {
}

runtime::Ref<AbyssRankDAO::RankingListPlayer> AbyssRankDAO::RankingListPlayer::create(int32_t position, int32_t oldPosition, int32_t id,
	std::string_view name, model::Race race, int32_t level, int32_t abyssRank, int32_t ap, int32_t gp, int32_t title, model::PlayerClass playerClass,
	model::Gender gender, std::string_view legionName) {
	return runtime::makeRef<RankingListPlayer>(position, oldPosition, id, name, race, level, abyssRank, ap, gp, title, playerClass, gender, legionName);
}

bool AbyssRankDAO::RankingListPlayer::equals(const RankingListPlayer& obj) const {
	return position_ == obj.position_ && oldPosition_ == obj.oldPosition_ && id_ == obj.id_ && name_ == obj.name_ && race_ == obj.race_
		&& level_ == obj.level_ && abyssRank_ == obj.abyssRank_ && ap_ == obj.ap_ && gp_ == obj.gp_ && title_ == obj.title_
		&& playerClass_ == obj.playerClass_ && gender_ == obj.gender_ && legionName_ == obj.legionName_;
}

int32_t AbyssRankDAO::RankingListPlayer::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units; enum components hash by ordinal (see enumHash)
	int32_t h = position_;
	h = detail::combineHash(h, oldPosition_);
	h = detail::combineHash(h, id_);
	h = detail::combineHash(h, detail::javaStringHashCode(name_));
	h = detail::combineHash(h, enumHash(race_));
	h = detail::combineHash(h, level_);
	h = detail::combineHash(h, abyssRank_);
	h = detail::combineHash(h, ap_);
	h = detail::combineHash(h, gp_);
	h = detail::combineHash(h, title_);
	h = detail::combineHash(h, enumHash(playerClass_));
	h = detail::combineHash(h, enumHash(gender_));
	h = detail::combineHash(h, detail::javaStringHashCode(legionName_));
	return h;
}

AbyssRankDAO::RankingListPlayer::~RankingListPlayer() = default;

AbyssRankDAO::RankingListLegion::RankingListLegion(int32_t position, int32_t oldPosition, int32_t id, std::string_view name, model::Race race,
	int32_t level, int64_t contributionPoints, int32_t memberCount)
	: position_(position), oldPosition_(oldPosition), id_(id), name_(name), race_(race), level_(level), contributionPoints_(contributionPoints),
		memberCount_(memberCount) {
}

runtime::Ref<AbyssRankDAO::RankingListLegion> AbyssRankDAO::RankingListLegion::create(int32_t position, int32_t oldPosition, int32_t id,
	std::string_view name, model::Race race, int32_t level, int64_t contributionPoints, int32_t memberCount) {
	return runtime::makeRef<RankingListLegion>(position, oldPosition, id, name, race, level, contributionPoints, memberCount);
}

bool AbyssRankDAO::RankingListLegion::equals(const RankingListLegion& obj) const {
	return position_ == obj.position_ && oldPosition_ == obj.oldPosition_ && id_ == obj.id_ && name_ == obj.name_ && race_ == obj.race_
		&& level_ == obj.level_ && contributionPoints_ == obj.contributionPoints_ && memberCount_ == obj.memberCount_;
}

int32_t AbyssRankDAO::RankingListLegion::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units, Long.hashCode; enum components hash by ordinal
	int32_t h = position_;
	h = detail::combineHash(h, oldPosition_);
	h = detail::combineHash(h, id_);
	h = detail::combineHash(h, detail::javaStringHashCode(name_));
	h = detail::combineHash(h, enumHash(race_));
	h = detail::combineHash(h, level_);
	h = detail::combineHash(h, detail::javaLongHashCode(contributionPoints_));
	h = detail::combineHash(h, memberCount_);
	return h;
}

AbyssRankDAO::RankingListLegion::~RankingListLegion() = default;

runtime::Ref<model::gameobjects::player::AbyssRank> AbyssRankDAO::loadAbyssRank(int32_t playerId) {
	runtime::Ref<AbyssRank> abyssRank;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_QUERY);
		stmt->setInt(1, playerId);
		auto rs = stmt->executeQuery();
		if (rs->next()) {
			int32_t daily_ap = rs->getInt("daily_ap");
			int32_t weekly_ap = rs->getInt("weekly_ap");
			int32_t ap = rs->getInt("ap");
			int32_t rank = rs->getInt("rank");
			int32_t daily_kill = rs->getInt("daily_kill");
			int32_t weekly_kill = rs->getInt("weekly_kill");
			int32_t all_kill = rs->getInt("all_kill");
			int32_t max_rank = rs->getInt("max_rank");
			int32_t last_kill = rs->getInt("last_kill");
			int32_t last_ap = rs->getInt("last_ap");
			int64_t last_update = rs->getLong("last_update");
			int32_t daily_gp = rs->getInt("daily_gp");
			int32_t weekly_gp = rs->getInt("weekly_gp");
			int32_t gp = rs->getInt("gp");
			int32_t last_gp = rs->getInt("last_gp");
			abyssRank = AbyssRank::create(daily_ap, weekly_ap, ap, rank, daily_kill, weekly_kill, all_kill, max_rank, last_kill, last_ap, last_update,
				daily_gp, weekly_gp, gp, last_gp);
			abyssRank->setPersistentState(Persistable::PersistentState::UPDATED);
		} else {
			abyssRank = AbyssRank::create(0, 0, 0, 1, 0, 0, 0, 1, 0, 0, commons::utils::currentTimeMillis(), 0, 0, 0, 0);
			abyssRank->setPersistentState(Persistable::PersistentState::NEW);
		}
	} catch (const SQLException& e) {
		log.error("Couldn't load abyss rank for player " + std::to_string(playerId), e);
	}
	return abyssRank;
}

void AbyssRankDAO::loadAbyssRank(model::gameobjects::player::Player& player) {
	runtime::Ref<AbyssRank> rank = loadAbyssRank(player.getObjectId());
	player.setAbyssRank(rank);
}

bool AbyssRankDAO::storeAbyssRank(model::gameobjects::player::Player& player) {
	runtime::Ptr<AbyssRank> rank = player.getAbyssRank();
	bool result = false;
	switch (rank->getPersistentState()) {
		case Persistable::PersistentState::NEW:
			result = insertRank(player.getObjectId(), *rank);
			break;
		case Persistable::PersistentState::UPDATE_REQUIRED:
			result = updateRank(player.getObjectId(), *rank);
			break;
		default:
			break;
	}
	rank->setPersistentState(Persistable::PersistentState::UPDATED);
	return result;
}

bool AbyssRankDAO::insertRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(INSERT_QUERY);
		stmt->setInt(1, playerId);
		stmt->setInt(2, rank.getDailyAP());
		stmt->setInt(3, rank.getWeeklyAP());
		stmt->setInt(4, rank.getAp());
		stmt->setInt(5, model::gameobjects::detail::abyssRankId(rank.getRank()));
		stmt->setInt(6, rank.getDailyKill());
		stmt->setInt(7, rank.getWeeklyKill());
		stmt->setInt(8, rank.getAllKill());
		stmt->setInt(9, rank.getMaxRank());
		stmt->setInt(10, rank.getLastKill());
		stmt->setInt(11, rank.getLastAP());
		stmt->setLong(12, rank.getLastUpdate());
		stmt->setInt(13, rank.getDailyGP());
		stmt->setInt(14, rank.getWeeklyGP());
		stmt->setInt(15, rank.getCurrentGP());
		stmt->setInt(16, rank.getLastGP());
		stmt->execute();
		return true;
	} catch (const SQLException& e) {
		log.error("Couldn't insert abyss rank for player " + std::to_string(playerId), e);
		return false;
	}
}

bool AbyssRankDAO::updateRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_QUERY);
		stmt->setInt(1, rank.getDailyAP());
		stmt->setInt(2, rank.getWeeklyAP());
		stmt->setInt(3, rank.getAp());
		stmt->setInt(4, model::gameobjects::detail::abyssRankId(rank.getRank()));
		stmt->setInt(5, rank.getDailyKill());
		stmt->setInt(6, rank.getWeeklyKill());
		stmt->setInt(7, rank.getAllKill());
		stmt->setInt(8, rank.getMaxRank());
		stmt->setInt(9, rank.getLastKill());
		stmt->setInt(10, rank.getLastAP());
		stmt->setLong(11, rank.getLastUpdate());
		stmt->setInt(12, rank.getDailyGP());
		stmt->setInt(13, rank.getWeeklyGP());
		stmt->setInt(14, rank.getCurrentGP());
		stmt->setInt(15, rank.getLastGP());
		stmt->setInt(16, playerId);
		stmt->execute();
		return true;
	} catch (const SQLException& e) {
		log.error("Couldn't update abyss rank of player " + std::to_string(playerId), e);
		return false;
	}
}

void AbyssRankDAO::dailyUpdateGp(utils::stats::AbyssRankEnum rank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(DECREASE_GP_DAILY);
		stmt->setInt(1, getGpLossPerDay(rank));
		stmt->setInt(2, model::gameobjects::detail::abyssRankId(rank));
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Couldn't decrease daily GP for rank " + detail::enumName(rank), e);
	}
}

void AbyssRankDAO::addGp(int32_t playerObjId, int32_t additionalGp, bool modifyStats) {
	std::string_view updateQuery = modifyStats ? INCREASE_GP_QUERY_WITH_STATS : INCREASE_GP_QUERY;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(updateQuery);
		if (modifyStats) {
			stmt->setInt(1, additionalGp);
			stmt->setInt(2, additionalGp);
			stmt->setInt(3, additionalGp);
			stmt->setInt(4, playerObjId);
		} else {
			stmt->setInt(1, additionalGp);
			stmt->setInt(2, playerObjId);
		}
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Couldn't increase {} GP for player {}", additionalGp, playerObjId, e);
	}
}

std::vector<runtime::Ref<AbyssRankDAO::RankingListPlayer>> AbyssRankDAO::loadRankingListPlayers() {
	std::vector<runtime::Ref<RankingListPlayer>> results;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_RANKING_LIST_PLAYERS);
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			int32_t position = rs->getInt("a.rank_pos");
			int32_t oldPosition = rs->getInt("a.old_rank_pos");
			int32_t id = rs->getInt("p.id");
			std::string name = rs->getString("p.name");
			model::Race race = detail::enumValueOf<model::Race>(rs->getString("p.race"), "com.aionemu.gameserver.model.Race");
			int32_t level = dataholders::DataManager::PLAYER_EXPERIENCE_TABLE->getLevelForExp(rs->getLong("p.exp"));
			int32_t rank = rs->getInt("a.rank");
			int32_t ap = rs->getInt("a.ap");
			int32_t gp = rs->getInt("a.gp");
			int32_t title = rs->getInt("p.title_id");
			model::PlayerClass playerClass = detail::enumValueOf<model::PlayerClass>(rs->getString("p.player_class"), "com.aionemu.gameserver.model.PlayerClass");
			model::Gender gender = detail::enumValueOf<model::Gender>(rs->getString("p.gender"), "com.aionemu.gameserver.model.Gender");
			std::string legionName = rs->getString("l.name");
			results.push_back(RankingListPlayer::create(position, oldPosition, id, name, race, level, rank, ap, gp, title, playerClass, gender, legionName));
		}
	} catch (const SQLException& e) {
		log.error("null", e); // Java: log.error(null, e)
	}
	return results;
}

std::vector<runtime::Ref<AbyssRankDAO::RankingListLegion>> AbyssRankDAO::loadRankingListLegions() {
	std::vector<runtime::Ref<RankingListLegion>> results;
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_RANKING_LIST_LEGIONS);
		auto rs = stmt->executeQuery();
		while (rs->next()) {
			int32_t position = rs->getInt("l.rank_pos");
			int32_t oldPosition = rs->getInt("l.old_rank_pos");
			int32_t id = rs->getInt("l.id");
			std::string name = rs->getString("l.name");
			model::Race race = detail::enumValueOf<model::Race>(rs->getString("p.race"), "com.aionemu.gameserver.model.Race");
			int32_t level = rs->getInt("l.level");
			int64_t contributionPoints = rs->getLong("l.contribution_points");
			int32_t memberCount = loadLegionMemberCount(*con, id);
			results.push_back(RankingListLegion::create(position, oldPosition, id, name, race, level, contributionPoints, memberCount));
		}
	} catch (const SQLException& e) {
		log.error("null", e); // Java: log.error(null, e)
	}
	return results;
}

int32_t AbyssRankDAO::loadLegionMemberCount(commons::database::Connection& con, int32_t legionId) {
	try {
		auto stmt = con.prepareStatement(SELECT_LEGION_COUNT);
		stmt->setInt(1, legionId);
		auto rs = stmt->executeQuery();
		rs->next();
		return rs->getInt("players");
	} catch (const SQLException& e) {
		log.error("Couldn't load legion member count for legion " + std::to_string(legionId), e);
		return 0;
	}
}

std::optional<std::vector<AbyssRankDAO::RankingListPlayerGp>> AbyssRankDAO::loadRankingListPlayersGp(model::Race race) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_RANKING_LIST_PLAYERS_GP);
		stmt->setString(1, detail::enumName(race));
		auto rs = stmt->executeQuery();
		std::vector<RankingListPlayerGp> rankingList;
		while (rs->next()) {
			int32_t rankPos = rs->getInt("rank_pos");
			int32_t playerId = rs->getInt("player_id");
			int32_t gp = rs->getInt("gp");
			rankingList.emplace_back(rankPos, playerId, gp);
		}
		return rankingList;
	} catch (const SQLException& e) {
		log.error("Couldn't load top ranks for race " + detail::enumName(race), e);
		return std::nullopt;
	}
}

std::optional<std::unordered_map<int32_t, int32_t>> AbyssRankDAO::loadApOfPlayersNotInRankingList(model::Race race,
	utils::stats::AbyssRankEnum minRank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(SELECT_UNRANKED_PLAYERS_AP);
		stmt->setString(1, detail::enumName(race));
		stmt->setInt(2, model::gameobjects::detail::abyssRankId(minRank));
		auto rs = stmt->executeQuery();
		std::unordered_map<int32_t, int32_t> apByPlayerId;
		while (rs->next())
			apByPlayerId.insert_or_assign(rs->getInt("player_id"), rs->getInt("ap"));
		return apByPlayerId;
	} catch (const SQLException& e) {
		log.error("Couldn't load ranks for race " + detail::enumName(race) + " (minRank " + detail::enumName(minRank) + ")", e);
		return std::nullopt;
	}
}

void AbyssRankDAO::updateAbyssRank(int32_t playerId, utils::stats::AbyssRankEnum rank) {
	try {
		auto con = DatabaseFactory::getConnection();
		auto stmt = con->prepareStatement(UPDATE_RANK);
		stmt->setInt(1, model::gameobjects::detail::abyssRankId(rank));
		stmt->setInt(2, playerId);
		stmt->execute();
	} catch (const SQLException& e) {
		log.error("Couldn't update abyss rank of player " + std::to_string(playerId) + " to " + detail::enumName(rank), e);
	}
}

void AbyssRankDAO::updateRankingLists(int32_t maxOfflineDays, int32_t playerLimit, int32_t legionLimit) {
	try {
		auto con = DatabaseFactory::getConnection();
		{
			auto stmt = con->prepareStatement(RESET_RANKING_LIST_PLAYERS);
			stmt->executeUpdate();
		}
		{
			auto stmt = con->prepareStatement(UPDATE_RANKING_LIST_PLAYERS_POSITIONS);
			if (maxOfflineDays > 0)
				stmt->addBatch("SET @minLastOnline = CURDATE() - INTERVAL " + std::to_string(maxOfflineDays) + " DAY;");
			stmt->addBatch("SET @a = 0;");
			stmt->setString(1, "ELYOS");
			stmt->setInt(2, playerLimit);
			stmt->addBatch();
			stmt->addBatch("SET @a = 0;");
			stmt->setString(1, "ASMODIANS");
			stmt->setInt(2, playerLimit);
			stmt->addBatch();
			if (maxOfflineDays > 0)
				stmt->addBatch("SET @minLastOnline = NULL;");
			stmt->executeBatch();
		}
		{
			auto stmt = con->prepareStatement(RESET_RANKING_LIST_LEGIONS);
			stmt->executeUpdate();
		}
		{
			auto stmt = con->prepareStatement(UPDATE_RANKING_LIST_LEGIONS_POSITIONS);
			stmt->addBatch("SET @a = 0;");
			stmt->setString(1, "ELYOS");
			stmt->setInt(2, legionLimit);
			stmt->addBatch();
			stmt->addBatch("SET @a = 0;");
			stmt->setString(1, "ASMODIANS");
			stmt->setInt(2, legionLimit);
			stmt->addBatch();
			stmt->executeBatch();
		}
	} catch (const SQLException& e) {
		log.error("null", e); // Java: log.error(null, e)
	}
}

} // namespace aion::gameserver::dao
