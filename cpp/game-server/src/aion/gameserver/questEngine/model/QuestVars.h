#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/questEngine/model/fwd.h"

namespace aion::gameserver::questEngine::model {

/**
 * The six 6-bit variables of a quest state.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `QuestState::questVars`), created with create(). The
 * constructor and its helper setVar (pure arithmetic) are ported, so QuestState objects can be created.
 *
 * @author MrPoke
 */
class QuestVars : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<runtime::Array<int32_t>> questVars;

protected:
	QuestVars();
	explicit QuestVars(int32_t var);
	~QuestVars() override;

public:
	/** Java: new QuestVars() */
	static runtime::Ref<QuestVars> create();

	/** Java: new QuestVars(var) */
	static runtime::Ref<QuestVars> create(int32_t var);

	/**
	 * @return Quest var by id.
	 */
	int32_t getVarById(int32_t id);

	void setVarById(int32_t id, int32_t var);

	/**
	 * @return int value of all values, stored in the array. Representation: Sum(value_on_index_i * 64^i)
	 */
	int32_t getQuestVars();

	/**
	 * Fill the array with values, based on the value represented like above
	 */
	void setVar(int32_t var);
};

} // namespace aion::gameserver::questEngine::model
