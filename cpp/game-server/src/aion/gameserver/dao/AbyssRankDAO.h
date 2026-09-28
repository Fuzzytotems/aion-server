#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/Connection.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author ATracer, Divinity, nrg
 */
class AbyssRankDAO {
public:
	class RankingListPlayerGp;
	class RankingListPlayer;
	class RankingListLegion;
	class RankingListPlayerGp {
	private:
		int32_t position_{};
		int32_t playerId_{};
		int32_t gp_{};
	public:
		RankingListPlayerGp(int32_t position, int32_t playerId, int32_t gp); // canonical record constructor
		int32_t position() const { return this->position_; }
		int32_t playerId() const { return this->playerId_; }
		int32_t gp() const { return this->gp_; }
		/** Java record equals: all components */
		bool equals(const RankingListPlayerGp& obj) const;
		int32_t hashCode() const;
	};
	class RankingListPlayer : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const int32_t position_{};
		const int32_t oldPosition_{};
		const int32_t id_{};
		const std::string name_{};
		const model::Race race_{};
		const int32_t level_{};
		const int32_t abyssRank_{};
		const int32_t ap_{};
		const int32_t gp_{};
		const int32_t title_{};
		const model::PlayerClass playerClass_{};
		const model::Gender gender_{};
		const std::string legionName_{};
	protected:
		RankingListPlayer(int32_t position, int32_t oldPosition, int32_t id, std::string_view name, model::Race race, int32_t level, int32_t abyssRank,
			// canonical record constructor
			int32_t ap, int32_t gp, int32_t title, model::PlayerClass playerClass, model::Gender gender, std::string_view legionName);
	public:
		static runtime::Ref<AbyssRankDAO::RankingListPlayer> create(int32_t position, int32_t oldPosition, int32_t id, std::string_view name,
			model::Race race, int32_t level, int32_t abyssRank, int32_t ap, int32_t gp, int32_t title, model::PlayerClass playerClass, model::Gender gender,
			std::string_view legionName);
		int32_t position() const { return this->position_; }
		int32_t oldPosition() const { return this->oldPosition_; }
		int32_t id() const { return this->id_; }
		std::string name() const { return this->name_; }
		model::Race race() const { return this->race_; }
		int32_t level() const { return this->level_; }
		int32_t abyssRank() const { return this->abyssRank_; }
		int32_t ap() const { return this->ap_; }
		int32_t gp() const { return this->gp_; }
		int32_t title() const { return this->title_; }
		model::PlayerClass playerClass() const { return this->playerClass_; }
		model::Gender gender() const { return this->gender_; }
		std::string legionName() const { return this->legionName_; }
		/** Java record equals: all components */
		bool equals(const RankingListPlayer& obj) const;
		int32_t hashCode() const;
	protected:
		~RankingListPlayer() override;
	};
	class RankingListLegion : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const int32_t position_{};
		const int32_t oldPosition_{};
		const int32_t id_{};
		const std::string name_{};
		const model::Race race_{};
		const int32_t level_{};
		const int64_t contributionPoints_{};
		const int32_t memberCount_{};
	protected:
		RankingListLegion(int32_t position, int32_t oldPosition, int32_t id, std::string_view name, model::Race race, int32_t level,
			int64_t contributionPoints, int32_t memberCount); // canonical record constructor
	public:
		static runtime::Ref<AbyssRankDAO::RankingListLegion> create(int32_t position, int32_t oldPosition, int32_t id, std::string_view name,
			model::Race race, int32_t level, int64_t contributionPoints, int32_t memberCount);
		int32_t position() const { return this->position_; }
		int32_t oldPosition() const { return this->oldPosition_; }
		int32_t id() const { return this->id_; }
		std::string name() const { return this->name_; }
		model::Race race() const { return this->race_; }
		int32_t level() const { return this->level_; }
		int64_t contributionPoints() const { return this->contributionPoints_; }
		int32_t memberCount() const { return this->memberCount_; }
		/** Java record equals: all components */
		bool equals(const RankingListLegion& obj) const;
		int32_t hashCode() const;
	protected:
		~RankingListLegion() override;
	};
public:
	static runtime::Ref<model::gameobjects::player::AbyssRank> loadAbyssRank(int32_t playerId);
	static void loadAbyssRank(model::gameobjects::player::Player& player);
	static bool storeAbyssRank(model::gameobjects::player::Player& player);
private:
	static bool insertRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank);
	static bool updateRank(int32_t playerId, model::gameobjects::player::AbyssRank& rank);
public:
	static void dailyUpdateGp(utils::stats::AbyssRankEnum rank);
	static void addGp(int32_t playerObjId, int32_t additionalGp, bool modifyStats);
	static std::vector<runtime::Ref<AbyssRankDAO::RankingListPlayer>> loadRankingListPlayers();
	static std::vector<runtime::Ref<AbyssRankDAO::RankingListLegion>> loadRankingListLegions();
private:
	static int32_t loadLegionMemberCount(commons::database::Connection& con, int32_t legionId);
public:
	/** @return null on an SQL error (AbyssRankUpdateService.java:80 skips the update) */
	static std::optional<std::vector<AbyssRankDAO::RankingListPlayerGp>> loadRankingListPlayersGp(model::Race race);
	/** @return null on an SQL error */
	static std::optional<std::unordered_map<int32_t, int32_t>> loadApOfPlayersNotInRankingList(model::Race race, utils::stats::AbyssRankEnum minRank);
	static void updateAbyssRank(int32_t playerId, utils::stats::AbyssRankEnum rank);
	static void updateRankingLists(int32_t maxOfflineDays, int32_t playerLimit, int32_t legionLimit);
};

} // namespace aion::gameserver::dao
