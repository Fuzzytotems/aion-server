#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * C++: an Immortal singleton (hub-headers.md §11.2) with a private constructor and destructor; getInstance() is Java's SingletonHolder.
 * objOwnerId is Java Integer (std::optional, hub-headers.md §6).
 *
 * @author Sarynth, nrg, Sykra
 */
class KiskService : public runtime::Immortal {
private:
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::Kisk>> boundButOfflinePlayer{AION_LOCK_CLASS(KiskService::boundButOfflinePlayer#stripe)}; // Java: = new ConcurrentHashMap<>()
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<model::gameobjects::Kisk>> ownerPlayer{AION_LOCK_CLASS(KiskService::ownerPlayer#stripe)}; // Java: = new ConcurrentHashMap<>()
	KiskService();
	~KiskService();
public:
	/** Remove kisk references and containers. */
	void removeKisk(model::gameobjects::Kisk& kisk);
	void onBind(model::gameobjects::Kisk& kisk, model::gameobjects::player::Player& player);
	void onLogin(model::gameobjects::player::Player& player);
	void onLogout(model::gameobjects::player::Player& player);
	void regKisk(model::gameobjects::Kisk& kisk, std::optional<int32_t> objOwnerId);
	bool haveKisk(std::optional<int32_t> objOwnerId);
	static KiskService& getInstance(); // Java singleton
};

} // namespace aion::gameserver::services
