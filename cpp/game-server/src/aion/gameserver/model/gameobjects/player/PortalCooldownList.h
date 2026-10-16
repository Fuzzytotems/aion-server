#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`const std::unique_ptr<PortalCooldownList>`), bound to the player
 * in the constructor. The cooldown map is created lazily and replaced by the DAO (setPortalCoolDowns): `Field<Ref<RcHashMap>>`, passed and
 * returned as `Ptr` (null until the first cooldown, as in Java).
 *
 * @author ATracer
 */
class PortalCooldownList : public runtime::OwnedPart {
private:
	runtime::OwnerRef<Player> owner;
	runtime::Field<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>>> portalCooldowns{};

public:
	/** Java package-private */
	explicit PortalCooldownList(Player& owner);

	~PortalCooldownList() override;

	bool isPortalUseDisabled(int32_t worldId);

	int64_t getPortalCooldownTime(int32_t worldId);

	/** @return the cooldown of the world, null if there is none */
	runtime::Ptr<PortalCooldown> getPortalCooldown(int32_t worldId);

	/** @return the cooldown of the world, null if the instance has no entrance cooltime */
	runtime::Ptr<PortalCooldown> getOrCreatePortalCooldown(int32_t worldId);

private:
	/** synchronized */
	runtime::Ptr<PortalCooldown> getOrCreatePortalCooldown(int32_t worldId, int64_t reuseTime);

public:
	runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> getPortalCoolDowns() const { return portalCooldowns.get(); }

	void setPortalCoolDowns(runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<PortalCooldown>>> portalCoolDowns);

	void addPortalCooldown(int32_t worldId, int64_t useDelay);

	void sendEntryInfo(int32_t worldId);

	void removePortalCooldown(int32_t worldId);

	bool hasCooldowns();

	int32_t size();
};

} // namespace aion::gameserver::model::gameobjects::player
