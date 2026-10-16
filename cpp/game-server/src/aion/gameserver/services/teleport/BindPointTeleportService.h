#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/templates/hotspot/fwd.h"
#include "aion/gameserver/services/teleport/fwd.h"

namespace aion::gameserver::services::teleport {

/**
 * C++: a static-only class (hub-headers.md §11.1).
 *
 * @author ViAl
 */
class BindPointTeleportService {
private:
	class Cooldown : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	public:
		runtime::Field<int32_t> locId{};
		runtime::Field<int64_t> cdEnd{};
	protected:
		Cooldown(int32_t locId, int64_t cdEnd);

	public:
		static runtime::Ref<BindPointTeleportService::Cooldown> create(int32_t value, int64_t cdEndValue);
		// Java protected: public, the enclosing class calls them (hub-headers.md §9.3)
		int32_t getLocId() const { return this->locId.get(); }
		int32_t getTimeLeft();

	protected:
		~Cooldown() override;
	};
	static constexpr int32_t COOLDOWN_IN_SECONDS = 600;
	static inline runtime::HashMap<int32_t, runtime::Ref<BindPointTeleportService::Cooldown>> cooldowns{AION_LOCK_CLASS(BindPointTeleportService::cooldowns)}; // Java: = new HashMap<>()
public:
	static void onLogin(model::gameobjects::player::Player& player);
	static void teleport(model::gameobjects::player::Player& player, int32_t locId, int64_t kinah);
	static void cancelTeleport(model::gameobjects::player::Player& player, int32_t locId);
private:
	static int64_t calculateTeleportationPrice(model::gameobjects::player::Player& player, const model::templates::hotspot::HotspotTemplate* hotspot, int64_t priceSentByGameClient);
	static bool checkRequirements(model::gameobjects::player::Player& player, const model::templates::hotspot::HotspotTemplate* hotspot, int64_t price);
	static void addCooldown(model::gameobjects::player::Player& player, int32_t locId);
	static runtime::Ptr<BindPointTeleportService::Cooldown> getCooldown(model::gameobjects::player::Player& player);
};

} // namespace aion::gameserver::services::teleport
