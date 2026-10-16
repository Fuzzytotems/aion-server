#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/fwd.h"

namespace aion::gameserver::controllers::observer {

/**
 * Remembers whether the effector moved while casting.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted ActionObserver (fieldmap K4, `Skill::moveListener`), created with
 * create(). The Java bodies are field reads and a literal store, ported inline.
 *
 * @author ATracer
 */
class StartMovingListener : public ActionObserver {
	AION_MAKE_REF_FRIEND
private:
	runtime::Field<bool> effectorMoved{false};

protected:
	StartMovingListener();
	~StartMovingListener() override;

public:
	/** Java: new StartMovingListener() */
	static runtime::Ref<StartMovingListener> create();

	/**
	 * @return the effectorMoved
	 */
	bool isEffectorMoved() const { return effectorMoved.get(); }

	void moved() override { effectorMoved.set(true); }
};

} // namespace aion::gameserver::controllers::observer
