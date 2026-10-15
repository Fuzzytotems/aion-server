#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/event/fwd.h"
#include "aion/gameserver/model/templates/event/upgradearcade/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * @author ginho1, Neon, Estrayl
 */
class SM_UPGRADE_ARCADE : public AionServerPacket {
private:
	int32_t action{};
	runtime::Ref<model::event::ArcadeProgress> progress{};
	int32_t sessionId{};
	bool showIcon{};
	bool success{};
	int32_t frenzyDurationSeconds{};
	bool disableWindow{};
	int32_t rewardItemId{};
	int64_t rewardItemCount{};
	std::vector<const model::templates::event::upgradearcade::ArcadeRewards*> arcadeRewards{};
public:
	SM_UPGRADE_ARCADE();
	explicit SM_UPGRADE_ARCADE(bool showIcon);
	SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progress, int32_t sessionId);
	SM_UPGRADE_ARCADE(bool success, model::event::ArcadeProgress& progress);
	explicit SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progress);
	SM_UPGRADE_ARCADE(model::event::ArcadeProgress& progress, bool resumeAllowed);
	SM_UPGRADE_ARCADE(int32_t itemId, int64_t count);
	explicit SM_UPGRADE_ARCADE(int32_t frenzyDurationSeconds);
	SM_UPGRADE_ARCADE(int32_t action, bool disableWindow);
	explicit SM_UPGRADE_ARCADE(const std::vector<const model::templates::event::upgradearcade::ArcadeRewards*>& rewards);
	~SM_UPGRADE_ARCADE() override;
protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
