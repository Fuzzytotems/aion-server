#pragma once

#include <cstdint>

#include "aion/gameserver/network/aion/AionClientPacket.h"

namespace aion::gameserver::network::aion::clientpackets {

/**
 * The player deletes a recipe from his recipe book (C_RECIPE_DELETE): the recipe id.
 * <p>
 * C++ only: `CM_RECIPE_DELETETestAccess` (tests/cm_lz/RecipePacketsTest.cpp) reads the field readImpl decoded, which Java keeps package-private
 * without a getter.
 *
 * @author Rolandas
 */
class CM_RECIPE_DELETE : public AionClientPacket {
	friend struct CM_RECIPE_DELETETestAccess;

private:
	int32_t recipeId{};

public:
	CM_RECIPE_DELETE(int32_t opcode, const StateSet& validStates);

protected:
	void readImpl() override;
	void runImpl() override;
};

} // namespace aion::gameserver::network::aion::clientpackets
