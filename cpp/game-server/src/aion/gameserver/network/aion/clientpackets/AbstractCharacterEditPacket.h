#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The character name, gender, race, class and appearance data shared by the character creation and edit packets.
 * <p>
 * C++: playerClass is std::optional (Java null for an invalid class id with ignoreInvalidPlayerClass, e.g. the random data of the type 1
 * CM_CREATE_CHARACTER).
 *
 * @author Neon
 */
class AbstractCharacterEditPacket : public AionClientPacket {
protected:
	std::string characterName;
	model::Gender gender{};
	model::Race race{};
	std::optional<model::PlayerClass> playerClass;
	runtime::Ref<model::gameobjects::player::PlayerAppearance> playerAppearance;

public:
	AbstractCharacterEditPacket(int32_t opcode, const StateSet& validStates);
	~AbstractCharacterEditPacket() override;

protected:
	void readBasicInfo(bool ignoreInvalidPlayerClass);
	void readAppearance();
};

} // namespace aion::gameserver::network::aion::clientpackets
