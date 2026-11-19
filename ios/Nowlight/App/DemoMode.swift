import Foundation
import WidgetKit

/// DEMO MODU: gercek kopru ve cihaz olmadan uygulamanin tamamini denemek icin. Uygulama icinde calisan sahte bir kopru ve kapi dugumu
/// (URLProtocol) vardir; ayarlar, eslestirme, servo, PIR, programlar, otomasyon, kopru guncellemesi gercekmis gibi calisir. Ag kullanmaz.
/// Acilinca gercek ayarlarin (adres, adlar, roller, programlar...) yedegi alinir, kapatilinca geri yuklenir.
enum DemoMode {
    static let host = "demo.nowlight"
    private static let flag = "demoMode"
    private static let backupKey = "demoBackup"
    private static let absentKey = "demoAbsent"
    private static var shared: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }
    /// Gercek ayarlari tutan anahtarlar (App Group)
    private static let keys = ["host", "token", "deviceNames", "deviceIcons", "customDevices", "removedDevices", "deviceRoles", "roleSettings",
                               "relayConfig", "bridgeLegacy", "doorHost", "doorToken", "doorRules", "scheduleRules", "deviceChips"]

    static var isOn: Bool { UserDefaults.standard.bool(forKey: flag) }
    /// Bos demo: hic cihaz yok, kopru da henuz tanitilmamis (token bos): ilk kurulumu bastan dene.
    static var isEmpty: Bool { UserDefaults.standard.bool(forKey: "demoEmpty") }

    /// Uygulama acilirken: sahte sunucuyu etkinlestir (demo aciksa).
    static func register() {
        URLProtocol.registerClass(DemoProtocol.self)
        if isOn { DemoState.shared.start() }
    }

    /// empty: true -> cihaz yok, token bos (ilk kurulum akisi: "Koprüyü otomatik bul" -> Cihaz ekle); false -> ornek cihazlarla.
    static func enable(empty: Bool = false) {
        guard !isOn else { return }
        UserDefaults.standard.set(empty, forKey: "demoEmpty")
        var backup: [String: Any] = [:], absent: [String] = []
        for k in keys { if let v = shared.object(forKey: k) { backup[k] = v } else { absent.append(k) } }
        UserDefaults.standard.set(backup, forKey: backupKey)
        UserDefaults.standard.set(absent, forKey: absentKey)
        if empty {          // ilk kurulum: genel kopru (yerlesik isik yok), cihaz yok, token bos
            for k in ["deviceNames", "deviceIcons", "customDevices", "removedDevices", "deviceRoles", "roleSettings", "relayConfig", "doorRules", "scheduleRules", "deviceChips", "doorHost", "doorToken"] { shared.removeObject(forKey: k) }
            shared.set(host, forKey: "host"); shared.removeObject(forKey: "token")
            shared.set(false, forKey: "bridgeLegacy")
            UserDefaults.standard.set(true, forKey: flag)
            DemoState.shared.reset(empty: true); DemoState.shared.start()
            WidgetCenter.shared.reloadAllTimelines()
            return
        }
        // Ornek cihazlar (kisisel kopru gibi: masa, yatak, led + servo + PIR)
        shared.set(host, forKey: "host"); shared.set("demo", forKey: "token")
        shared.set(["d3": 3, "d4": 4], forKey: "deviceChips")
        shared.set(["masa": "Salon", "yatak": "Yatak odası", "led": "Mutfak", "d3": "Perde", "d4": "Kapı sensörü"], forKey: "deviceNames")
        shared.set(["masa": "lamp.floor.fill", "yatak": "bed.double.fill", "led": "fork.knife", "d3": "window.shade.closed", "d4": "figure.walk.motion"], forKey: "deviceIcons")
        shared.set(["d3", "d4"], forKey: "customDevices")
        shared.set(["d3": "servo", "d4": "pir"], forKey: "deviceRoles")
        shared.set(["d3": ServoSettings(pin: 5, off: 0, on: 90, speed: 3, release: 10).stored, "d4": PirSettings(pin: 4).stored], forKey: "roleSettings")
        shared.removeObject(forKey: "removedDevices"); shared.removeObject(forKey: "relayConfig")
        shared.set(true, forKey: "bridgeLegacy")
        shared.set(host, forKey: "doorHost"); shared.set("demo", forKey: "doorToken")
        shared.removeObject(forKey: "doorRules"); shared.removeObject(forKey: "scheduleRules")
        UserDefaults.standard.set(true, forKey: flag)
        DemoState.shared.reset(); DemoState.shared.start()
        WidgetCenter.shared.reloadAllTimelines()
    }

    static func disable() {
        guard isOn else { return }
        let backup = UserDefaults.standard.dictionary(forKey: backupKey) ?? [:]
        let absent = UserDefaults.standard.stringArray(forKey: absentKey) ?? []
        for k in keys { if let v = backup[k] { shared.set(v, forKey: k) } else if absent.contains(k) { shared.removeObject(forKey: k) } }
        UserDefaults.standard.removeObject(forKey: backupKey); UserDefaults.standard.removeObject(forKey: absentKey)
        UserDefaults.standard.set(false, forKey: flag)
        UserDefaults.standard.set(false, forKey: "demoEmpty")
        DemoState.shared.stop()
        StateCache.clear()
        WidgetCenter.shared.reloadAllTimelines()
    }
}

/// Sahte koprunun ve kapi dugumunun durumu (bellekte).
final class DemoState: @unchecked Sendable {
    static let shared = DemoState()
    private let lock = NSLock()
    private var values: [String: Int] = [:]
    private var roles: [String: String] = [:]
    struct ServoSim { var angle: Int; var free: Bool; var off: Int; var on: Int; var release: Int; var speed = 3; var from = 0; var started = Date.distantPast; var dur = 0.0
        /// Su anki aci ve hareket ediyor mu (zamana gore dogrusal)
        func now() -> (cur: Int, moving: Bool) {
            let e = Date().timeIntervalSince(started)
            if dur <= 0 || e >= dur { return (angle, false) }
            return (from + Int(Double(angle - from) * e / dur), true)
        }
        mutating func go(to a: Int) { from = now().cur; angle = a; free = false; started = Date(); dur = speed == 0 ? 0.8 : Double(abs(a - from)) / Double(speed * 10) }
    }
    private var servo: [String: ServoSim] = [:]
    private var schedules = ""
    private var tz = 180
    private var doorRules = ""
    private var doorEnabled = true
    private var pairStarted: Date?
    private var pairedID: String?
    private var version = "0.6.0", latest = "0.6.0", updState = "idle", updProgress = 0
    private var motionTimer: Timer?
    private var initialized = false               // reset() en az bir kez calisti mi (bos demoda values bos kalir, bu yuzden ayri bayrak)
    private var legacy = true                      // bos demoda genel kopru: yerlesik isik yok
    private var pairRound = 0                      // her eslesme turunda farkli islemci (rehberi gormek icin)
    private var schedTimer: Timer?
    private var schedFired: [Int: Int64] = [:]       // kural sirasi -> en son tetiklendigi dakika (ayni dakikada iki kez calismasin)

    func reset(empty: Bool = false) {
        lock.lock(); defer { lock.unlock() }
        legacy = !empty; initialized = true
        values = empty ? [:] : ["masa": 1, "yatak": 0, "led": 0, "d3": 0, "d4": 0]
        roles = empty ? [:] : ["d3": "servo", "d4": "pir"]
        servo = empty ? [:] : ["d3": ServoSim(angle: 0, free: true, off: 0, on: 90, release: 10)]
        pairRound = 0
        schedules = ""; tz = 180; doorRules = ""; doorEnabled = true; pairStarted = nil; pairedID = nil
        version = "0.6.0"; latest = "0.6.0"; updState = "idle"; updProgress = 0
    }

    /// PIR hareketini taklit eder: ~20 sn'de bir 6 sn hareket var.
    func start() {
        if !initialized { reset(empty: DemoMode.isEmpty) }      // uygulama yeniden acilinca bos demo bos kalsin
        DispatchQueue.main.async { [self] in
            schedTimer?.invalidate()
            schedTimer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in self?.tickSchedules() }
            motionTimer?.invalidate()
            motionTimer = Timer.scheduledTimer(withTimeInterval: 20, repeats: true) { [weak self] _ in
                guard let self else { return }
                self.lock.lock(); if self.roles["d4"] == "pir" { self.values["d4"] = 1 }; self.lock.unlock()
                DispatchQueue.main.asyncAfter(deadline: .now() + 6) { self.lock.lock(); if self.roles["d4"] == "pir" { self.values["d4"] = 0 }; self.lock.unlock() }
            }
        }
    }
    func stop() { DispatchQueue.main.async { [self] in motionTimer?.invalidate(); motionTimer = nil; schedTimer?.invalidate(); schedTimer = nil } }

    // MARK: yonlendirme
    /// (durum kodu, govde)
    func handle(path: String, method: String, query: [String: String], form: [String: String]) -> (Int, Any) {
        lock.lock(); defer { lock.unlock() }
        let q = query.merging(form) { a, _ in a }
        let post = method == "POST"
        switch path {
        case "/api/ping": return (200, ["name": "nowlight-bridge"])
        case "/api/setup": return (200, ["name": "nowlight-bridge", "ip": DemoMode.host, "token": "demo"])      // ip: demo adresi kalsin
        case "/api/state": return (200, stateBody())
        case "/api/set" where post:
            let dev = q["dev"] ?? "", on = q["val"] == "1"
            for d in dev == "all" ? Array(values.keys) : [dev] where values[d] != nil {
                if dev == "all", let r = roles[d], r != "light" { continue }
                if roles[d] == "pir" { return (400, ["error": "rol"]) }
                values[d] = on ? 1 : 0
                if var s = servo[d] { s.go(to: on ? s.on : s.off); servo[d] = s; scheduleRelease(d) }
            }
            return (200, ["ok": true])
        case "/api/servo" where post:
            let id = q["id"] ?? "", a = Int(q["angle"] ?? "0") ?? 0
            if var s = servo[id] { s.go(to: a); servo[id] = s; values[id] = abs(a - s.on) <= abs(a - s.off) ? 1 : 0; scheduleRelease(id) }
            return (200, ["ok": true])
        case "/api/servo/free" where post:
            if let id = q["id"], var s = servo[id] { s.free = true; servo[id] = s }
            return (200, ["ok": true])
        case "/api/node/servo" where post:
            let id = q["id"] ?? ""
            if var s = servo[id] { s.off = Int(q["off"] ?? "") ?? s.off; s.on = Int(q["on"] ?? "") ?? s.on; s.release = Int(q["release"] ?? "") ?? s.release; s.speed = Int(q["speed"] ?? "") ?? s.speed; servo[id] = s }
            return (200, ["ok": true])
        case "/api/node/pir", "/api/node/config": return (200, ["ok": true])
        case "/api/node/forget" where post:
            if let id = q["id"] { values.removeValue(forKey: id); roles.removeValue(forKey: id); servo.removeValue(forKey: id) }
            return (200, ["ok": true, "restarting": true])
        case "/api/pair/start" where post: pairStarted = Date(); pairRound += 1; pairedID = nil; return (200, pairBody())
        case "/api/pair": return (200, pairBody())
        case "/api/pair/stop" where post: pairStarted = nil; return (200, ["ok": true])
        case "/api/pair/accept" where post:
            let id = q["id"] ?? "d5"
            values[id] = 0; pairedID = id
            let r = q["role"] ?? "light"
            if r != "light" { roles[id] = r }
            if r == "servo" { servo[id] = ServoSim(angle: 0, free: true, off: 0, on: 90, release: 10) }
            return (200, ["ok": true])
        case "/api/schedules" where post:
            let w = q["rules"] ?? ""
            if !validSchedules(w) { return (400, ["error": "kurallar gecersiz"]) }
            schedules = w; tz = Int(q["tz"] ?? "") ?? tz; schedFired = [:]
            return (200, schedBody())
        case "/api/schedules": return (200, schedBody())
        case "/api/version": return (200, ["version": version, "chip": "ESP32-S3", "ota": true, "generic": true])
        case "/api/update": return (200, updBody())
        case "/api/update/check" where post: if updState == "idle" { latest = "0.6.1" }; return (200, updBody())
        case "/api/update/start" where post: startUpdate(); return (200, updBody())
        // ---- kapi dugumu
        case "/api/status": return (200, doorStatus())
        case "/api/config" where post: if let e = q["enabled"] { doorEnabled = e == "1" }; return (200, doorStatus())
        case "/api/rules" where post:
            let w = q["rules"] ?? ""
            if w.split(separator: ";").count > 8 { return (400, ["error": "kurallar gecersiz"]) }
            doorRules = w; return (200, doorStatus())
        case "/api/rules": return (200, ["rules": doorRules])
        case "/api/test" where post: return (200, doorStatus())
        default: return (404, ["error": "yok"])
        }
    }

    // MARK: programlar (gercek koprudeki motorun taklidi: saat, gun, dakika)
    private func tickSchedules() {
        lock.lock(); defer { lock.unlock() }
        guard !schedules.isEmpty else { return }
        let now = Date(), c = Calendar.current.dateComponents([.hour, .minute, .weekday], from: now)
        let minute = (c.hour ?? 0) * 60 + (c.minute ?? 0)
        let wday = ((c.weekday ?? 2) + 5) % 7                       // Pazartesi = 0 ... Pazar = 6
        let key = Int64(now.timeIntervalSince1970 / 60)
        for (i, item) in schedules.split(separator: ";").enumerated() {
            let v = item.split(separator: ":").compactMap { Int($0) }
            guard v.count == 6, v[0] != 0, v[3] == minute, (v[4] >> wday) & 1 == 1, schedFired[i] != key else { continue }
            schedFired[i] = key
            let ids = ["masa", "yatak", "led", "d3", "d4", "d5", "d6", "d7"]
            for slot in 0..<8 where (v[2] >> slot) & 1 == 1 {
                let id = ids[slot]
                guard values[id] != nil, roles[id] != "pir" else { continue }
                if v[1] == 3 { if var s = servo[id] { s.go(to: v[5]); servo[id] = s; values[id] = abs(v[5] - s.on) <= abs(v[5] - s.off) ? 1 : 0; scheduleRelease(id) }; continue }
                let on = v[1] == 1 ? true : v[1] == 2 ? false : values[id] != 1
                values[id] = on ? 1 : 0
                if var s = servo[id] { s.go(to: on ? s.on : s.off); servo[id] = s; scheduleRelease(id) }
            }
        }
    }

    // MARK: govdeler
    private func stateBody() -> [String: Any] {
        var devices: [String: Int] = [:], allRoles: [String: String] = [:], sv: [String: Any] = [:]
        for (k, v) in values where !["masa", "yatak", "led"].contains(k) { devices[k] = v; allRoles[k] = roles[k] ?? "light" }
        for (k, s) in servo { let n = s.now(); sv[k] = ["angle": s.angle, "free": s.free && !n.moving, "moving": n.moving, "cur": n.cur] }
        return ["masa": values["masa"] ?? (legacy ? 0 : -1), "yatak": values["yatak"] ?? (legacy ? 0 : -1), "led": values["led"] ?? (legacy ? 0 : -1), "devices": devices,
                "links": Dictionary(uniqueKeysWithValues: values.keys.map { ($0, true) }), "roles": allRoles, "servo": sv,
                "legacy": legacy, "channel": 6, "wifiChannel": 6]
    }
    private func pairBody() -> [String: Any] {
        guard let t = pairStarted else { return ["open": false, "secondsLeft": 0, "found": []] }
        let el = Date().timeIntervalSince(t)
        // Her eslesme turunda farkli islemci: S3, C3, klasik ESP32, C6, S2 (pin rehberini gormek icin)
        let chips = [3, 4, 1, 5, 2], chip = chips[(pairRound - 1 + chips.count) % chips.count]
        let found: [[String: Any]] = el > 4 && pairedID == nil ? [["mac": "AA:BB:CC:11:22:\(String(format: "%02X", 0x30 + chip))", "name": "Nowlight-11\(chip)\(chip)", "rssi": -52, "chip": chip, "pin": chip == 1 ? 23 : 5, "inv": 1]] : []
        return ["open": el < 120, "secondsLeft": max(0, 120 - Int(el)), "found": found]
    }
    private func schedBody() -> [String: Any] {
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        return ["rules": schedules, "tz": tz, "synced": true, "time": f.string(from: Date()), "count": schedules.split(separator: ";").count]
    }
    private func updBody() -> [String: Any] {
        ["state": updState, "current": version, "latest": latest, "available": latest != version, "progress": updProgress, "error": ""]
    }
    private func doorStatus() -> [String: Any] {
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        return ["enabled": doorEnabled ? 1 : 0, "pir": 0, "motions": 7, "motionAge": 95, "trigAge": 600, "timeSynced": 1, "time": f.string(from: Date()),
                "bridgeSet": 1, "lastResult": "henuz tetiklenmedi", "rules": doorRules.split(separator: ";").count, "activeRule": -1]
    }
    private func validSchedules(_ w: String) -> Bool {
        if w.isEmpty { return true }
        let items = w.split(separator: ";")
        if items.count > 16 { return false }
        for i in items { let v = i.split(separator: ":").compactMap { Int($0) }; if v.count != 6 || !(1...127).contains(v[4]) || !(0...1439).contains(v[3]) { return false } }
        return true
    }
    private func scheduleRelease(_ id: String) {
        guard let s = servo[id], s.release > 0 else { return }
        let target = s.angle
        DispatchQueue.global().asyncAfter(deadline: .now() + s.dur + Double(s.release) / 10.0 + 0.3) { [self] in
            lock.lock(); if var c = servo[id], c.angle == target { c.free = true; servo[id] = c }; lock.unlock()
        }
    }
    private func startUpdate() {
        guard latest != version, updState != "downloading" else { return }
        updState = "downloading"; updProgress = 0
        DispatchQueue.global().async { [self] in
            for p in stride(from: 5, through: 100, by: 5) { Thread.sleep(forTimeInterval: 0.35); lock.lock(); updProgress = p; lock.unlock() }
            lock.lock(); updState = "done"; version = latest; lock.unlock()
            Thread.sleep(forTimeInterval: 3); lock.lock(); updState = "idle"; lock.unlock()
        }
    }
}

/// Demo adresine (demo.nowlight) giden istekleri agda degil, DemoState'ten cevaplar.
final class DemoProtocol: URLProtocol {
    override class func canInit(with request: URLRequest) -> Bool { request.url?.host == DemoMode.host }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }

    override func startLoading() {
        guard let url = request.url else { return }
        var query: [String: String] = [:]
        URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?.forEach { query[$0.name] = $0.value ?? "" }
        var form: [String: String] = [:]
        var body = request.httpBody
        if body == nil, let s = request.httpBodyStream {
            s.open(); defer { s.close() }
            var d = Data(); var buf = [UInt8](repeating: 0, count: 2048)
            while s.hasBytesAvailable { let n = s.read(&buf, maxLength: buf.count); if n <= 0 { break }; d.append(buf, count: n) }
            body = d
        }
        if let b = body, let str = String(data: b, encoding: .utf8) {
            var c = URLComponents(); c.percentEncodedQuery = str
            c.queryItems?.forEach { form[$0.name] = $0.value ?? "" }
        }
        let needsAuth = !["/api/ping", "/api/setup"].contains(url.path)
        var code = 200; var obj: Any = [:]
        if needsAuth && request.value(forHTTPHeaderField: "X-Token") != "demo" { code = 401; obj = ["error": "token"] }
        else { (code, obj) = DemoState.shared.handle(path: url.path, method: request.httpMethod ?? "GET", query: query, form: form) }
        let data = (try? JSONSerialization.data(withJSONObject: obj)) ?? Data()
        let resp = HTTPURLResponse(url: url, statusCode: code, httpVersion: "HTTP/1.1", headerFields: ["Content-Type": "application/json"])!
        DispatchQueue.global().asyncAfter(deadline: .now() + 0.12) { [self] in      // ag gecikmesi gibi
            client?.urlProtocol(self, didReceive: resp, cacheStoragePolicy: .notAllowed)
            client?.urlProtocol(self, didLoad: data)
            client?.urlProtocolDidFinishLoading(self)
        }
    }
    override func stopLoading() {}
}
