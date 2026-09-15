#pragma once

#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"

namespace aion::gameserver::dataholders::detail {

/**
 * C++ only: the holder registry of the 92 StaticData fields (generated AION_STATIC_DATA_HOLDERS, Java StaticData's @XmlElement list) with the
 * hook dependency table of static-data.md §3.3. The binder translation unit of the holders: game code never includes their .bind.h headers.
 * <p>
 * Thread-safety: created once on first use (thread-safe static initialization), immutable afterwards.
 */
const xml::HolderRegistry& staticDataRegistry();

} // namespace aion::gameserver::dataholders::detail
