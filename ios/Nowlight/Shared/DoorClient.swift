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
    var mode: Int          // 0 cevir, 1 yalnizca ac, 2 yalnizca kapat
    var lamps: Int         // bit0 masa, bit1 yatak, bit2 led
    var cooldown: Int      // saniye
    var winOn: Bool
    var winStart: Int      // dakika
    var winEnd: Int
    var pir: Int
    var motions: Int
    var motionAge: Int
    var trigAge: Int
    var timeSynced: Bool
    var time: String
    var inWindow: Bool
    var bridgeSet: Bool
    var lastResult: String

    // Cihaz bu alanlari 0/1 sayi olarak yollar.
    enum CodingKeys: String, CodingKey { case enabled, mode, lamps, cooldown, winOn, winStart, winEnd, pir, motions, motionAge, trigAge, timeSynced, time, inWindow, bridgeSet, lastResult }
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        func b(_ k: CodingKeys) throws -> Bool { try c.decode(Int.self, forKey: k) != 0 }
        enabled = try b(.enabled); winOn = try b(.winOn); timeSynced = try b(.timeSynced); inWindow = try b(.inWindow); bridgeSet = try b(.bridgeSet)
        mode = try c.decode(Int.self, forKey: .mode); lamps = try c.decode(Int.self, forKey: .lamps)
        cooldown = try c.decode(Int.self, forKey: .cooldown); winStart = try c.decode(Int.self, forKey: .winStart); winEnd = try c.decode(Int.self, forKey: .winEnd)
        pir = try c.decode(Int.self, forKey: .pir); motions = try c.decode(Int.self, forKey: .motions)
        motionAge = try c.decode(Int.self, forKey: .motionAge); trigAge = try c.decode(Int.self, forKey: .trigAge)
        time = try c.decode(String.self, forKey: .time); lastResult = try c.decode(String.self, forKey: .lastResult)
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
    func test() async throws -> DoorStatus { try decode(await send(request("/api/test", method: "POST"))) }
}
