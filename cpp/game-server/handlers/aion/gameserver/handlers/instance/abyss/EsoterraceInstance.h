#pragma once

#include "aion/gameserver/handlers/instance/InstancePrelude.h"

namespace aion::gameserver::handlers::instance::abyss {

/**
 * Esoterrace (300250000): Keening Sirokin and his treasure guard at 21 %, the doors the bosses' deaths open, the Surkana Feeder's hard mode,
 * the revive at the entrance and the Drana production lab's message.
 * <p>
 * Java: data/handlers/instance/abyss/EsoterraceInstance.java, @InstanceID(300250000).
 *
 * @author xTz, Gigi
 */
class EsoterraceInstance : public GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
protected:
	explicit EsoterraceInstance(WorldMapInstance& instance);

public:
	static runtime::Ref<EsoterraceInstance> create(WorldMapInstance& instance);

	void onInstanceCreate() override;

	void onDie(Npc& npc) override;

	bool onReviveEvent(Player& player) override;

	void onEnterZone(Player& player, ZoneInstance& zone) override;
};

} // namespace aion::gameserver::handlers::instance::abyss
