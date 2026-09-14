package instance;

import com.aionemu.gameserver.instance.handlers.GeneralInstanceHandler;
import com.aionemu.gameserver.instance.handlers.InstanceID;
import com.aionemu.gameserver.world.WorldMapInstance;

@InstanceID(300120000)
public class NotPortedInstance extends GeneralInstanceHandler {

	public NotPortedInstance(WorldMapInstance instance) {
		super(instance);
	}
}
