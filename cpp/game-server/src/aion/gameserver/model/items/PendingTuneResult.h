#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/items/fwd.h"

namespace aion::gameserver::model::items {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `Item.pendingTuneResult`), created with create().
 *
 * @author Estrayl
 */
class PendingTuneResult : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t optionalSockets;
	const int32_t enchantBonus;
	const int32_t statBonusId;
	const bool attributeOnly;

protected:
	PendingTuneResult(int32_t optionalSockets, int32_t enchantBonus, int32_t statBonusId, bool attributeOnly);
	~PendingTuneResult() override;

public:
	/** Java: new PendingTuneResult(optionalSockets, enchantBonus, statBonusId, attributeOnly) */
	static runtime::Ref<PendingTuneResult> create(int32_t optionalSockets, int32_t enchantBonus, int32_t statBonusId, bool attributeOnly);

	int32_t getOptionalSockets() const { return optionalSockets; }

	int32_t getEnchantBonus() const { return enchantBonus; }

	int32_t getStatBonusId() const { return statBonusId; }

	bool isAttributeOnly() const { return attributeOnly; }
};

} // namespace aion::gameserver::model::items
