import Foundation

/// Kapi dugumunun (firmware/door) ayarlari ve yerel HTTP istemcisi.
enum DoorConfig {
    private static var d: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }
    static var host: String {
        get { d.string(forKey: "doorHost") ?? "" }
        set { d.set(newValue.trimmingCharacters(in: .whitespaces), forKey: "doorHost") }
    }
    static var token: String {
        get { d.string(forKey: "doorToken") ?? "" }
        set { d.set(newValue.trimmingCharacters(in: .whitespacesAndNewlines), forKey: "doorToken") }
    }
    static var isConfigured: Bool { !host.isEmpty && !token.isEmpty }
}

struct DoorStatus: Decodable, Equatable {
    var enabled: Bool
    var pir: Int
    var motions: Int
    var motionAge: Int
    var trigAge: Int
    var timeSynced: Bool
    var time: String
    var bridgeSet: Bool
    var lastResult: String
    var rules: Int?          // eski yazilimda yok: kural destegi yok demek
    var activeRule: Int?

    enum CodingKeys: String, CodingKey { case enabled, pir, motions, motionAge, trigAge, timeSynced, time, bridgeSet, lastResult, rules, activeRule }
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        enabled = try c.decode(Int.self, forKey: .enabled) != 0
        timeSynced = try c.decode(Int.self, forKey: .timeSynced) != 0
        bridgeSet = try c.decode(Int.self, forKey: .bridgeSet) != 0
        pir = try c.decode(Int.self, forKey: .pir); motions = try c.decode(Int.self, forKey: .motions)
        motionAge = try c.decode(Int.self, forKey: .motionAge); trigAge = try c.decode(Int.self, forKey: .trigAge)
        time = try c.decode(String.self, forKey: .time); lastResult = try c.decode(String.self, forKey: .lastResult)
        rules = try c.decodeIfPresent(Int.self, forKey: .rules)
        activeRule = try c.decodeIfPresent(Int.self, forKey: .activeRule)
    }
}

/// Bir otomasyon kurali. Telefonda saklanir, kapi dugumune "en:eylem:isiklar:bas:bit:bekleme;..." olarak gonderilir (isim gonderilmez).
struct AutomationRule: Codable, Identifiable, Equatable {
    enum Action: Int, Codable, CaseIterable { case toggle = 0, on = 1, off = 2
        var title: String { switch self { case .toggle: "Çevir"; case .on: "Aç"; case .off: "Kapat" } }
    }
    var id = UUID()
    var name = "Yeni kural"
    var enabled = true
    var action: Action = .on
    var lamps = AutomationRule.visibleMask   // bit = 1 << isik yuvasi (masa 0, yatak 1, led 2, eklenenler 3...7)
    var start = 18 * 60           // dakika
    var end = 6 * 60              // bitis haric; gece yarisini asabilir; bas == bit: gun boyu
    var cooldownMin = 0           // 0...60

    /// Uygulamada gorunen (kaldirilmamis) isiklarin bit maskesi: masa 1, yatak 2, led 4.
    static var visibleMask: Int { LightID.devices.reduce(0) { $0 | $1.bit } }
    /// Kural yalnizca gorunen isiklara uygulanir; hic gorunen isik kalmadiysa dugumde kapali gonderilir.
    var effectiveLamps: Int { lamps & Self.visibleMask }

    var wire: String { "\(enabled && effectiveLamps != 0 ? 1 : 0):\(action.rawValue):\(effectiveLamps != 0 ? effectiveLamps : lamps):\(start):\(end):\(cooldownMin)" }

    static func parse(_ wire: String) -> [AutomationRule] {
        wire.split(separator: ";").enumerated().compactMap { i, item in
            let v = item.split(separator: ":").compactMap { Int($0) }
            guard v.count == 6, let a = Action(rawValue: v[1]) else { return nil }
            return AutomationRule(name: "Kural \(i + 1)", enabled: v[0] != 0, action: a, lamps: v[2], start: v[3], end: v[4], cooldownMin: v[5])
        }
    }

    static func hhmm(_ m: Int) -> String { String(format: "%02d:%02d", m / 60, m % 60) }
    var windowText: String { start == end ? "Gün boyu" : "\(Self.hhmm(start)) – \(Self.hhmm(end))" }
    var lampText: String {
        let shown = LightID.allDevices.filter { effectiveLamps & $0.bit != 0 }
        if shown.isEmpty { return "Işık yok (kaldırılmış)" }
        return shown.count == LightID.devices.count && shown.count > 1 ? "Tüm ışıklar" : shown.map(\.title).joined(separator: ", ")
    }
}

struct DoorClient {
    let host: String
    let token: String

    private func request(_ path: String, method: String = "GET", form: [String: String] = [:], auth: Bool = true) throws -> URLRequest {
        var c = URLComponents()
        c.scheme = "http"
        let parts = host.split(separator: ":", maxSplits: 1)
        c.host = String(parts.first ?? "")
        if parts.count == 2, let p = Int(parts[1]) { c.port = p }
        c.path = path
        guard let url = c.url else { throw BridgeError.unreachable }
        var r = URLRequest(url: url, timeoutInterval: 6)    // kopru cagrisi (test) birkac sn surebilir
        r.httpMethod = method
        if auth { r.setValue(token, forHTTPHeaderField: "X-Token") }
        if !form.isEmpty {
            var q = URLComponents()
            q.queryItems = form.map { URLQueryItem(name: $0.key, value: $0.value) }
            r.httpBody = q.percentEncodedQuery?.data(using: .utf8)
            r.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        }
        return r
    }

    private func send(_ r: URLRequest) async throws -> Data {
        let data: Data, resp: URLResponse
        do { (data, resp) = try await URLSession.shared.data(for: r) }
        catch let e as URLError where e.code == .cancelled { throw CancellationError() }
        catch { throw BridgeError.unreachable }
        guard let http = resp as? HTTPURLResponse else { throw BridgeError.badResponse }
        if http.statusCode == 401 { throw BridgeError.unauthorized }
        guard (200..<300).contains(http.statusCode) else { throw BridgeError.badResponse }
        return data
    }

    private func decode(_ d: Data) throws -> DoorStatus {
        guard let s = try? JSONDecoder().decode(DoorStatus.self, from: d) else { throw BridgeError.badResponse }
        return s
    }

    func ping() async throws -> Bool {
        let d = try await send(request("/api/ping", auth: false))
        return String(data: d, encoding: .utf8)?.contains("nowlight-door") == true
    }
    func status() async throws -> DoorStatus { try decode(await send(request("/api/status"))) }
    func configure(_ f: [String: String]) async throws -> DoorStatus { try decode(await send(request("/api/config", method: "POST", form: f))) }
    func test(rule: Int) async throws -> DoorStatus { try decode(await send(request("/api/test", method: "POST", form: ["rule": String(rule)]))) }
    func pushRules(_ rules: [AutomationRule]) async throws -> DoorStatus {
        try decode(await send(request("/api/rules", method: "POST", form: ["rules": rules.map(\.wire).joined(separator: ";")])))
    }
    func pullRules() async throws -> [AutomationRule] {
        struct R: Decodable { let rules: String }
        let d = try await send(request("/api/rules"))
        guard let r = try? JSONDecoder().decode(R.self, from: d) else { throw BridgeError.badResponse }
        return AutomationRule.parse(r.rules)
    }
}
