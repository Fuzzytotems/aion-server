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
 * C++: the header includes AbyssRankDAO.h for the nested record type RankingListPlayer (a nested class cannot be forward-declared).
 *
 * @author Rhys2002, zdead, LokiReborn
 */
class SM_ABYSS_RANKING_PLAYERS : public AionServerPacket {
private:
	std::vector<runtime::Ref<dao::AbyssRankDAO::RankingListPlayer>> players{};
	int32_t lastUpdate{};
	int32_t race{};
	int32_t page{};
	bool isEndPacket{};

public:
	SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdate, model::Race race);
	SM_ABYSS_RANKING_PLAYERS(int32_t lastUpdate, const std::vector<runtime::Ptr<dao::AbyssRankDAO::RankingListPlayer>>& players, model::Race race,
		int32_t page, bool isEndPacket);
	~SM_ABYSS_RANKING_PLAYERS() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
