#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/Expirable.h"
#include "aion/gameserver/model/gameobjects/player/emotion/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player::emotion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K3, `EmotionList.emotions`), created with create(). Expirable is
 * held by Ref, so retain()/release() forward to RefCounted (§9.2).
 *
 * @author MrPoke
 */
class Emotion : public runtime::RefCounted, public Expirable {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id;
	const int32_t expireTime;

protected:
	Emotion(int32_t id, int32_t expireTime);
	~Emotion() override;

public:
	/** Java: new Emotion(id, expireTime) */
	static runtime::Ref<Emotion> create(int32_t id, int32_t expireTime);

	/** C++ only: Ref<Expirable> retains this object (hub-headers.md §9.2). */
	void retain() const noexcept override { runtime::RefCounted::retain(); }

	void release() const noexcept override { runtime::RefCounted::release(); }

	int32_t getId() const { return id; }

	int32_t getExpireTime() override { return expireTime; }

	void onExpire(Player& player) override;
};

} // namespace aion::gameserver::model::gameobjects::player::emotion
