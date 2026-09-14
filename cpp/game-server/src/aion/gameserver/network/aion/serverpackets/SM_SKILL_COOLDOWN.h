#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionServerPacket.h"
#include "aion/gameserver/network/aion/fwd.h"
#include "aion/gameserver/network/aion/serverpackets/fwd.h"

namespace aion::gameserver::network::aion::serverpackets {

/**
 * S0c declaration header (hub-headers.md §12). C++ difference: the private record Cooldown is the element type of the packet member, so it is
 * defined in the class (RefCounted, fieldmap K3; record components with a trailing underscore next to their accessors).
 *
 * @author ATracer, nrg
 */
class SM_SKILL_COOLDOWN : public AionServerPacket {
private:
	/** Java: private record Cooldown(int skillId, long expirationTimeMillis) */
	class Cooldown : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const int32_t skillId_;
		const int64_t expirationTimeMillis_;

	protected:
		Cooldown(int32_t skillId, int64_t expirationTimeMillis);
		~Cooldown() override;

	public:
		/** Java: new Cooldown(skillId, expirationTimeMillis) (canonical record constructor) */
		static runtime::Ref<Cooldown> create(int32_t skillId, int64_t expirationTimeMillis);

		int32_t getRemainingSeconds();

		int32_t getDurationMillis();

		int32_t skillId() const { return skillId_; }

		int64_t expirationTimeMillis() const { return expirationTimeMillis_; }

		/** Java record equals (all components) */
		bool equals(const Cooldown& obj) const;

		/** Java record hashCode */
		int32_t hashCode() const;
	};

	std::vector<runtime::Ref<SM_SKILL_COOLDOWN::Cooldown>> cooldowns{}; // Java: = new ArrayList<>()
	bool notify{};

public:
	SM_SKILL_COOLDOWN(int32_t skillId, int64_t expirationTimeMillis);
	SM_SKILL_COOLDOWN(model::gameobjects::player::Player& player, const std::unordered_map<int32_t, int64_t>& cooldownExpirationMillisByCooldownId,
		bool notify);
	/** Creates a skill cooldown reset packet */
	SM_SKILL_COOLDOWN(model::gameobjects::player::Player& player, const std::vector<int32_t>& resettableCooldownIds);
	~SM_SKILL_COOLDOWN() override;

protected:
	void writeImpl(AionConnection* con) override;
};

} // namespace aion::gameserver::network::aion::serverpackets
