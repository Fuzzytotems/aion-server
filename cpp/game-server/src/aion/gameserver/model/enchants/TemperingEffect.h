#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/enchants/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"

namespace aion::gameserver::model::enchants {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `Item.temperingEffect`), created by apply() through the
 * private constructor (C++ create is private too). StatOwner is held by Ref, so retain()/release() forward to RefCounted (§9.2). The stat
 * function lists hold the newly created functions (`std::vector<Ref<IStatFunction>>`); the private helpers append to the caller's list.
 *
 * @author xTz
 */
class TemperingEffect : public runtime::RefCounted, public stats::calc::StatOwner {
	AION_MAKE_REF_FRIEND
private:
	// Java: LoggerFactory.getLogger(TemperingEffect.class) inline in apply() - namespace-scope logger in TemperingEffect.cpp
	TemperingEffect(gameobjects::player::Player& player, const std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions);

	/** Java: new TemperingEffect(player, functions) (private) */
	static runtime::Ref<TemperingEffect> create(gameobjects::player::Player& player,
		const std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions);

protected:
	~TemperingEffect() override;

public:
	/** C++ only: Ref<StatOwner> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	void endEffect(gameobjects::player::Player& player);

private:
	static void addAccessoryStatFunctions(gameobjects::Item& item, std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions);

	static void addPlumeStatFunctions(gameobjects::Item& item, std::vector<runtime::Ref<stats::calc::functions::IStatFunction>>& functions);

public:
	static void apply(gameobjects::player::Player& player, gameobjects::Item& item);
};

} // namespace aion::gameserver::model::enchants
