#include "aion/gameserver/dao/AbyssRankDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

namespace {

// Java SQL constants of the class, used only by the bodies (P4-14 ports them with the DAO methods).
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

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.AbyssRankDAO");

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
	// Java: String.hashCode/Enum identity hash of the components; ported with the first hash collection that holds the record
	AION_UNPORTED();
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
	// Java: String.hashCode/Enum identity hash of the components; ported with the first hash collection that holds the record
	AION_UNPORTED();
}

AbyssRankDAO::RankingListLegion::~RankingListLegion() = default;

runtime::Ref<model::gameobjects::player::AbyssRank> AbyssRankDAO::loadAbyssRank(int32_t playerId) {
	AION_UNPORTED();
}

void AbyssRankDAO::loadAbyssRank(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AbyssRankDAO::storeAbyssRank(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool AbyssRankDAO::insertRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank) {
	AION_UNPORTED();
}

bool AbyssRankDAO::updateRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank) {
	AION_UNPORTED();
}

void AbyssRankDAO::dailyUpdateGp(utils::stats::AbyssRankEnum rank) {
	AION_UNPORTED();
}

void AbyssRankDAO::addGp(int32_t playerObjId, int32_t additionalGp, bool modifyStats) {
	AION_UNPORTED();
}

std::vector<runtime::Ref<AbyssRankDAO::RankingListPlayer>> AbyssRankDAO::loadRankingListPlayers() {
	AION_UNPORTED();
}

std::vector<runtime::Ref<AbyssRankDAO::RankingListLegion>> AbyssRankDAO::loadRankingListLegions() {
	AION_UNPORTED();
}

int32_t AbyssRankDAO::loadLegionMemberCount(commons::database::Connection& con, int32_t legionId) {
	AION_UNPORTED();
}

std::optional<std::vector<AbyssRankDAO::RankingListPlayerGp>> AbyssRankDAO::loadRankingListPlayersGp(model::Race race) {
	AION_UNPORTED();
}

std::optional<std::unordered_map<int32_t, int32_t>> AbyssRankDAO::loadApOfPlayersNotInRankingList(model::Race race, utils::stats::AbyssRankEnum minRank) {
	AION_UNPORTED();
}

void AbyssRankDAO::updateAbyssRank(int32_t playerId, utils::stats::AbyssRankEnum rank) {
	AION_UNPORTED();
}

void AbyssRankDAO::updateRankingLists(int32_t maxOfflineDays, int32_t playerLimit, int32_t legionLimit) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
