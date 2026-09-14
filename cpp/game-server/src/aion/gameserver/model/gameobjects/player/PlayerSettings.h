#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `Player.playerSettings`), created with create(). The byte
 * arrays are stored and returned as they are (null allowed), so they are passed as the runtime Array like Player.captchaImage.
 *
 * @author ATracer
 */
class PlayerSettings : public runtime::RefCounted, public Persistable {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<PersistentState> persistentState{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> uiSettings{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> shortcuts{};
	runtime::Field<runtime::Ref<runtime::Array<int8_t>>> houseBuddies{};
	runtime::Field<int32_t> deny{0};
	runtime::Field<int32_t> display{0};

protected:
	PlayerSettings();

	PlayerSettings(runtime::Ptr<runtime::Array<int8_t>> uiSettings, runtime::Ptr<runtime::Array<int8_t>> shortcuts,
		runtime::Ptr<runtime::Array<int8_t>> houseBuddies, int32_t deny, int32_t display);

	~PlayerSettings() override;

public:
	/** Java: new PlayerSettings() */
	static runtime::Ref<PlayerSettings> create();

	/** Java: new PlayerSettings(uiSettings, shortcuts, houseBuddies, deny, display) */
	static runtime::Ref<PlayerSettings> create(runtime::Ptr<runtime::Array<int8_t>> uiSettings, runtime::Ptr<runtime::Array<int8_t>> shortcuts,
		runtime::Ptr<runtime::Array<int8_t>> houseBuddies, int32_t deny, int32_t display);

	PersistentState getPersistentState() override { return persistentState.get(); }

	void setPersistentState(PersistentState value) override { persistentState.set(value); }

	runtime::Ptr<runtime::Array<int8_t>> getUiSettings() const { return uiSettings.get(); }

	void setUiSettings(runtime::Ptr<runtime::Array<int8_t>> uiSettings);

	runtime::Ptr<runtime::Array<int8_t>> getShortcuts() const { return shortcuts.get(); }

	void setShortcuts(runtime::Ptr<runtime::Array<int8_t>> shortcuts);

	runtime::Ptr<runtime::Array<int8_t>> getHouseBuddies() const { return houseBuddies.get(); }

	void setHouseBuddies(runtime::Ptr<runtime::Array<int8_t>> houseBuddies);

	int32_t getDisplay() const { return display.get(); }

	void setDisplay(int32_t display);

	int32_t getDeny() const { return deny.get(); }

	void setDeny(int32_t deny);

	bool isInDeniedStatus(DeniedStatus deny);
};

} // namespace aion::gameserver::model::gameobjects::player
