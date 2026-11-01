#pragma once

// The independent SM_* decoders of F-08 live in the client protocol library (cpp/client-protocol, aion/clientprotocol/decoders), which the UE
// client shares; the scenario code keeps calling them decoders::X.

namespace aion::clientprotocol::decoders {
}

namespace aion::gameserver::scenario {

namespace decoders = clientprotocol::decoders;

} // namespace aion::gameserver::scenario
