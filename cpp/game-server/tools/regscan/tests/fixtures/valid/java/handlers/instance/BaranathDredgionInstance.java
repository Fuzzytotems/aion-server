package instance;

import com.aionemu.gameserver.instance.handlers.GeneralInstanceHandler;
import com.aionemu.gameserver.instance.handlers.InstanceID;
import com.aionemu.gameserver.world.WorldMapInstance;

@InstanceID(300110000)
public class BaranathDredgionInstance extends GeneralInstanceHandler {

	public BaranathDredgionInstance(WorldMapInstance instance) {
		super(instance);
	}
}
