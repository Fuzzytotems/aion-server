#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player::chainSkills`), created with create(); the Java
 * field initializers (`new ChainSkill("")`) run in the constructor's member initializer list.
 *
 * @author kecimis, Neon
 */
class ChainSkills : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<runtime::Ref<ChainSkill>> previousChainSkill{};
	runtime::Field<runtime::Ref<ChainSkill>> chainSkill{};
	runtime::Field<int64_t> expireTime{0};

protected:
	ChainSkills();
	~ChainSkills() override;

public:
	/** Java: new ChainSkills() */
	static runtime::Ref<ChainSkills> create();

	/**
	 * @return The chain skill used before the current one.
	 */
	runtime::Ptr<ChainSkill> getPreviousChainSkill() const { return previousChainSkill.get(); }

	/**
	 * @return The last used chain skill.
	 */
	runtime::Ptr<ChainSkill> getCurrentChainSkill() const { return chainSkill.get(); }

	/**
	 * @return Number of activations for the current chain skill. 0 if chain skill category doesn't match the current one, or no chain is active.
	 */
	int32_t getCurrentChainCount(std::string_view category);

	void updateChain(std::string_view category, int32_t duration);

	/**
	 * Resets the complete chain (clears all info).
	 */
	void resetChain();

	/**
	 * @return True if this chain is expired. It must be reset to make it usable again.
	 */
	bool isChainExpired();
};

} // namespace aion::gameserver::skillengine::model
