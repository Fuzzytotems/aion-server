#pragma once

#include <cstdint>
#include <string_view>
#include <map>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/dao/fwd.h"
#include "aion/gameserver/model/event/fwd.h"

namespace aion::gameserver::dao {

/**
 * @author Estrayl
 */
class HeadhuntingDAO {
public:
	/** C++: Java TreeMap */
	static std::map<int32_t, runtime::Ref<model::event::Headhunter>> loadHeadhunters();
	static bool clearTables();
	static void storeHeadhunter(int32_t hunterId);
};

} // namespace aion::gameserver::dao
