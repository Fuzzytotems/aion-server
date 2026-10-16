#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/services/toypet/fwd.h"

namespace aion::gameserver::services::toypet {

/**
 * @author ATracer
 */
class PetMoodService {
public:
	static void checkMood(model::gameobjects::Pet& pet, int32_t type, int32_t shuggleEmotion);
private:
	static void requestPresent(model::gameobjects::Pet& pet);
	static void interactWithPet(model::gameobjects::Pet& pet, int32_t shuggleEmotion);
	static void startCheckingMood(model::gameobjects::Pet& pet);
};

} // namespace aion::gameserver::services::toypet
