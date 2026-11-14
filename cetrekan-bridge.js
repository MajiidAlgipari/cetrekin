/*************************************************************
 * cetrekan-bridge.js
 * Jembatan web <-> ESP32 lewat MQTT over WebSocket (WSS).
 *
 * Pasang SEBELUM file ini di HTML kamu:
 *   <script src="https://cdn.jsdelivr.net/npm/mqtt@5.10.1/dist/mqtt.min.js"></script>
 *   <script src="cetrekan-bridge.js"></script>
 *
 * Pemakaian:
 *   Cetrekan.connect({
 *     host: "abc123.s1.eu.hivemq.cloud",
 *     port: 8884,                 // port WebSocket TLS (HiveMQ Cloud)
 *     user: "USER_WEB",
 *     pass: "PASSWORD_WEB",
 *     prefix: "cetrekan"          // sama dengan PREFIX di ESP32
 *   }, {
 *     onStatus:     (on)    => { ... },   // true = ON, false = OFF
 *     onOnline:     (ok)    => { ... },   // true = ESP32 online
 *     onConnection: (state) => { ... },   // "connected" | "reconnecting" | "disconnected"
 *     onError:      (msg)   => { ... }
 *   });
 *
 *   Cetrekan.press(true);    // kirim ON
 *   Cetrekan.press(false);   // kirim OFF
 *************************************************************/

const Cetrekan = (() => {
  let client = null;
  let topics = {};
  let h = {};

  const call = (fn, ...args) => { if (typeof fn === "function") fn(...args); };

  function connect(cfg, handlers) {
    h = handlers || {};
    const prefix = cfg.prefix || "cetrekan";
    topics = {
      cmd:    prefix + "/cmd",
      status: prefix + "/status",
      online: prefix + "/online",
    };

    const url = "wss://" + cfg.host + ":" + (cfg.port || 8884) + "/mqtt";
    client = mqtt.connect(url, {
      username: cfg.user,
      password: cfg.pass,
      clientId: "web-" + Math.random().toString(16).slice(2, 10),
      reconnectPeriod: 3000,
      connectTimeout: 8000,
      clean: true,
    });

    client.on("connect", () => {
      client.subscribe([topics.status, topics.online], { qos: 1 });
      call(h.onConnection, "connected");
    });
    client.on("reconnect", () => call(h.onConnection, "reconnecting"));
    client.on("close",     () => call(h.onConnection, "disconnected"));
    client.on("error",     (e) => call(h.onError, e && e.message ? e.message : String(e)));

    client.on("message", (topic, payload) => {
      const text = payload.toString();
      if (topic === topics.status) call(h.onStatus, text === "1");
      if (topic === topics.online) call(h.onOnline, text === "online");
    });
  }

  function press(on) {
    if (!client || !client.connected) return false;
    client.publish(topics.cmd, on ? "1" : "0", { qos: 1 });
    return true;
  }

  return { connect, press };
})();
