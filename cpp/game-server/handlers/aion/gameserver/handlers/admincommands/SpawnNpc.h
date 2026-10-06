#pragma once

#include "aion/gameserver/handlers/admincommands/AdminCommandsPrelude.h"

#include "aion/gameserver/model/gameobjects/HouseObject.h"

namespace aion::gameserver::handlers::admincommands {

/**
 * //spawn: spawns NPCs, gatherables and house objects.
 */
class SpawnNpc : public AdminCommand {
public:
	SpawnNpc();

	void execute(Player& player, std::span<const std::string> params) override;

private:
	void spawnHouseObject(Player& admin, int32_t itemId);

	/** Java: private static class DummyHouseObject extends HouseObject<PlaceableHouseObject> (no registry, an auto-released object ID) */
	class DummyHouseObject : public HouseObject {
		AION_MAKE_REF_FRIEND
	protected:
		DummyHouseObject(CreateKey key, int32_t templateId);
		~DummyHouseObject() override = default;

	public:
		float getX() override;
		float getY() override;
		float getZ() override;
		int8_t getHeading() override;
	};
};

} // namespace aion::gameserver::handlers::admincommands
