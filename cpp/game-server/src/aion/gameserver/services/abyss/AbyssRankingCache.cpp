#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_LEGION_EDIT.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::abyss {

using dao::AbyssRankDAO;
using network::aion::serverpackets::SM_ABYSS_RANKING_LEGIONS;
using network::aion::serverpackets::SM_ABYSS_RANKING_PLAYERS;

AbyssRankingCache::AbyssRankingCache() {
	refreshCache();
}

AbyssRankingCache::~AbyssRankingCache() = default;

AbyssRankingCache& AbyssRankingCache::getInstance() {
	static AbyssRankingCache instance; // Java SingletonHolder
	return instance;
}

void AbyssRankingCache::refreshCache() {
	const std::array<model::Race, 2> races{model::Race::ASMODIANS, model::Race::ELYOS};
	std::vector<runtime::Ref<AbyssRankDAO::RankingListPlayer>> rankingListPlayersValue = AbyssRankDAO::loadRankingListPlayers();
	std::vector<runtime::Ref<AbyssRankDAO::RankingListLegion>> rankingListLegionsValue = AbyssRankDAO::loadRankingListLegions();
	runtime::Ref<runtime::RcHashMap<model::Race, runtime::Ref<runtime::RcArrayList<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>>>>> newPlayerRankListPackets =
		runtime::RcHashMap<model::Race, runtime::Ref<runtime::RcArrayList<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>>>>::create(
			AION_LOCK_CLASS(AbyssRankingCache::playerRankListPackets));
	runtime::Ref<runtime::RcHashMap<model::Race, std::shared_ptr<SM_ABYSS_RANKING_LEGIONS>>> newLegionRankListPackets =
		runtime::RcHashMap<model::Race, std::shared_ptr<SM_ABYSS_RANKING_LEGIONS>>::create(AION_LOCK_CLASS(AbyssRankingCache::legionRankListPackets));

	int32_t updateTime = static_cast<int32_t>(commons::utils::currentTimeMillis() / 1000);

	for (model::Race race : races) {
		std::vector<runtime::Ptr<AbyssRankDAO::RankingListPlayer>> players;
		for (const runtime::Ref<AbyssRankDAO::RankingListPlayer>& p : rankingListPlayersValue) {
			if (p->race() == race)
				players.emplace_back(p);
		}
		runtime::Ref<runtime::RcArrayList<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>>> playerPackets =
			runtime::RcArrayList<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>>::create(AION_LOCK_CLASS(AbyssRankingCache::playerRankListPackets#list));
		playerPackets->addAll(getPlayerRankListPackets(updateTime, race, players));
		newPlayerRankListPackets->put(race, std::move(playerPackets));

		std::vector<runtime::Ptr<AbyssRankDAO::RankingListLegion>> legions;
		for (const runtime::Ref<AbyssRankDAO::RankingListLegion>& l : rankingListLegionsValue) {
			if (l->race() == race)
				legions.emplace_back(l);
		}
		newLegionRankListPackets->put(race, std::make_shared<SM_ABYSS_RANKING_LEGIONS>(updateTime, legions, race));
	}

	// assign the finished lists
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<AbyssRankDAO::RankingListPlayer>>> playersById =
		runtime::RcHashMap<int32_t, runtime::Ref<AbyssRankDAO::RankingListPlayer>>::create(AION_LOCK_CLASS(AbyssRankingCache::rankingListPlayers));
	for (const runtime::Ref<AbyssRankDAO::RankingListPlayer>& p : rankingListPlayersValue) {
		if (playersById->putIfAbsent(p->id(), p)) // Java Collectors.toMap: a duplicate key throws
			throw commons::utils::IllegalStateException("Duplicate key " + std::to_string(p->id()));
	}
	runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<AbyssRankDAO::RankingListLegion>>> legionsById =
		runtime::RcHashMap<int32_t, runtime::Ref<AbyssRankDAO::RankingListLegion>>::create(AION_LOCK_CLASS(AbyssRankingCache::rankingListLegions));
	for (const runtime::Ref<AbyssRankDAO::RankingListLegion>& l : rankingListLegionsValue) {
		if (legionsById->putIfAbsent(l->id(), l))
			throw commons::utils::IllegalStateException("Duplicate key " + std::to_string(l->id()));
	}
	this->rankingListPlayers.set(std::move(playersById));
	this->rankingListLegions.set(std::move(legionsById));
	this->playerRankListPackets.set(std::move(newPlayerRankListPackets));
	this->legionRankListPackets.set(std::move(newLegionRankListPackets));
	this->lastUpdate.set(updateTime);
}

void AbyssRankingCache::reloadRankings() {
	// update cache
	refreshCache();

	world::World::getInstance().forEachPlayer([](model::gameobjects::player::Player& player) {
		player.resetAbyssRankListUpdated();
		if (runtime::Ptr<model::team::legion::Legion> legion = player.getLegion()) // update legion rank number
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_LEGION_EDIT(0x01, *legion));
	});
}

std::vector<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>> AbyssRankingCache::getPlayerRankListPackets(int32_t updateTime, model::Race race,
	const std::vector<runtime::Ptr<AbyssRankDAO::RankingListPlayer>>& list) {
	std::vector<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>> playerPackets;
	int32_t page = 1;
	const size_t size = list.size();

	for (size_t i = 0; i < size; i += 44) {
		if (size > i + 44) {
			playerPackets.push_back(std::make_shared<SM_ABYSS_RANKING_PLAYERS>(updateTime,
				std::vector<runtime::Ptr<AbyssRankDAO::RankingListPlayer>>(list.begin() + i, list.begin() + i + 44), race, page, false));
		} else {
			playerPackets.push_back(std::make_shared<SM_ABYSS_RANKING_PLAYERS>(updateTime,
				std::vector<runtime::Ptr<AbyssRankDAO::RankingListPlayer>>(list.begin() + i, list.end()), race, page, true));
		}
		page++;
	}

	return playerPackets;
}

runtime::Ptr<runtime::RcArrayList<std::shared_ptr<SM_ABYSS_RANKING_PLAYERS>>> AbyssRankingCache::getPlayers(model::Race race) {
	return playerRankListPackets.get()->get(race);
}

std::shared_ptr<SM_ABYSS_RANKING_LEGIONS> AbyssRankingCache::getLegions(model::Race race) {
	return legionRankListPackets.get()->getOrDefault(race, nullptr);
}

int32_t AbyssRankingCache::getRankingListPosition(model::gameobjects::player::Player& player) {
	runtime::Ptr<AbyssRankDAO::RankingListPlayer> rank = rankingListPlayers.get()->get(player.getObjectId());
	return !rank ? 0 : rank->position();
}

int32_t AbyssRankingCache::getRankingListPosition(model::team::legion::Legion& legion) {
	runtime::Ptr<AbyssRankDAO::RankingListLegion> rank = rankingListLegions.get()->get(legion.getLegionId());
	return !rank ? 0 : rank->position();
}

} // namespace aion::gameserver::services::abyss
