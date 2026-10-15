#include "aion/gameserver/model/instance/instanceposition/DisciplineInstancePosition.h"

namespace aion::gameserver::model::instance::instanceposition {

DisciplineInstancePosition::DisciplineInstancePosition() = default;

DisciplineInstancePosition::~DisciplineInstancePosition() = default;

runtime::Ref<DisciplineInstancePosition> DisciplineInstancePosition::create() {
	return runtime::makeRef<DisciplineInstancePosition>();
}

// Java DisciplineInstancePosition.java:11-78
void DisciplineInstancePosition::port(gameobjects::player::Player& player, int32_t zone, int32_t position) {
	switch (position) {
		case 1:
			switch (zone) {
				case 1:
					teleport(player, 1841.294f, 1041.223f, 338.20056f, int8_t{15});
					break;
				case 2:
					teleport(player, 278.18478f, 1265.8389f, 263.1712f, int8_t{73});
					break;
				case 3:
					teleport(player, 709.78845f, 1766.1855f, 183.43953f, int8_t{60});
					break;
				case 4:
					teleport(player, 1817.1067f, 1737.4899f, 311.49692f, int8_t{1});
					break;
			}
			break;
		case 2:
			switch (zone) {
				case 1:
					teleport(player, 1869.4803f, 1041.8444f, 337.9918f, int8_t{43});
					break;
				case 2:
					teleport(player, 251.03516f, 1297.7039f, 248.11426f, int8_t{105});
					break;
				case 3:
					teleport(player, 693.93176f, 1761.0234f, 196.12753f, int8_t{21});
					break;
				case 4:
					teleport(player, 1851.6932f, 1765.4813f, 305.23187f, int8_t{90});
					break;
			}
			break;
		case 3:
			switch (zone) {
				case 1:
					teleport(player, 1869.0569f, 1069.1344f, 337.6657f, int8_t{71});
					break;
				case 2:
					teleport(player, 315.8269f, 1221.0648f, 263.4517f, int8_t{51});
					break;
				case 3:
					teleport(player, 686.09247f, 1756.8987f, 163.4386f, int8_t{25});
					break;
				case 4:
					teleport(player, 1851.7856f, 1709.3085f, 305.23566f, int8_t{31});
					break;
			}
			break;
		case 4:
			switch (zone) {
				case 1:
					teleport(player, 1841.7906f, 1069.6471f, 338.10706f, int8_t{107});
					break;
				case 2:
					teleport(player, 346.1267f, 1185.1802f, 244.43742f, int8_t{44});
					break;
				case 3:
					teleport(player, 693.11945f, 1771.6886f, 236.5583f, int8_t{17});
					break;
				case 4:
					teleport(player, 1887.0206f, 1737.6492f, 311.49692f, int8_t{62});
					break;
			}
			break;
	}
}

} // namespace aion::gameserver::model::instance::instanceposition
