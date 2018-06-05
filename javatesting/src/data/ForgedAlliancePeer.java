package data;

import lombok.Data;

import java.util.LinkedList;
import java.util.Queue;

@Data
public class ForgedAlliancePeer {

	private boolean connected = false;
	public final String remoteAddress;
	public final int remotePort;
	public final int remoteId;
	public final String remoteUsername;
	public final Offerer offerer;

	public int echoRequestsSent;
	public long lastPacketReceived = System.currentTimeMillis();
	public Queue<Integer> latencies = new LinkedList<>();

	public int addLatency(int lat) {
		lastPacketReceived = System.currentTimeMillis();

		latencies.add(lat);
		if(latencies.size() > 10) {
			latencies.remove();
		}
		return getLatency();
	}

	public int getLatency() {
		return (int)latencies.stream().mapToInt(Integer::intValue).average().orElse(0);
	}

	public int getJitter() {
		int lat = getLatency();
		return Math.max(latencies.stream().mapToInt(Integer::intValue).max().orElse(0) - lat, lat - latencies.stream().mapToInt(Integer::intValue).min().orElse(0));
	}

	public boolean isQuiet() {
		return System.currentTimeMillis() - lastPacketReceived > 5000;
	}

	public static enum Offerer {
		REMOTE, LOCAL
	}

}
