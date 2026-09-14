#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/team/legion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"
#include "aion/gameserver/services/abyss/fwd.h"

namespace aion::gameserver::services::abyss {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder. The
 * cached packets are shared (std::shared_ptr, as fieldmap.py maps packets stored in containers) and sent to many players
 * (runtime-architecture.md §8.2).
 *
 * @author VladimirZ, Neon
 */
class AbyssRankingCache : public runtime::Immortal {
private:
	runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<dao::AbyssRankDAO::RankingListPlayer>>>> rankingListPlayers{};
	runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<dao::AbyssRankDAO::RankingListLegion>>>> rankingListLegions{};
	runtime::Field<runtime::Ref<runtime::RcHashMap<model::Race, runtime::Ref<runtime::RcArrayList<std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS>>>>>> playerRankListPackets{};
	runtime::Field<runtime::Ref<runtime::RcHashMap<model::Race, std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_LEGIONS>>>> legionRankListPackets{};
	runtime::Field<int32_t> lastUpdate{};
	AbyssRankingCache();
	~AbyssRankingCache();
public:
	static AbyssRankingCache& getInstance(); // Java singleton
private:
	/** Loads ranking data from DB */
	void refreshCache();
public:
	/** Reloads player & legion rank data from DB and refreshes the in-game views */
	void reloadRankings();
private:
	std::vector<std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS>> getPlayerRankListPackets(int32_t updateTime, model::Race race, const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListPlayer>>& list);
public:
	/** @return the cached packets of the race (the list stored in playerRankListPackets) */
	runtime::Ptr<runtime::RcArrayList<std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS>>> getPlayers(model::Race race);
	/** @return the cached packet of the race */
	std::shared_ptr<network::aion::serverpackets::SM_ABYSS_RANKING_LEGIONS> getLegions(model::Race race);
	int32_t getRankingListPosition(model::gameobjects::player::Player& player);
	int32_t getRankingListPosition(model::team::legion::Legion& legion);
	int32_t getLastUpdate() const { return this->lastUpdate.get(); }
};

} // namespace aion::gameserver::services::abyss
