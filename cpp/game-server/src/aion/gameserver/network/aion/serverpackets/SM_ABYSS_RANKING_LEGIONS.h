#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/dao/AbyssRankDAO.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * C++: the header includes AbyssRankDAO.h for the nested record type RankingListLegion (a nested class cannot be forward-declared).
 *
 * @author zdead, LokiReborn
 */
class SM_ABYSS_RANKING_LEGIONS : public AionServerPacket {
private:
	std::vector<runtime::Ref<dao::AbyssRankDAO::RankingListLegion>> rankingList{};
	model::Race race{};
	int32_t updateTime{};
	bool clearListBeforeUpdate{};

public:
	SM_ABYSS_RANKING_LEGIONS(int32_t updateTime, model::Race race);
	SM_ABYSS_RANKING_LEGIONS(int32_t updateTime, const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListLegion>>& rankingList,
		model::Race race);

private:
	SM_ABYSS_RANKING_LEGIONS(int32_t updateTime, const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListLegion>>& rankingList, model::Race race,
		bool clearListBeforeUpdate);

public:
	~SM_ABYSS_RANKING_LEGIONS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
