import Foundation

enum BridgeError: LocalizedError {
    case notConfigured, unauthorized, unreachable, badResponse, setupClosed

    var errorDescription: String? {
        switch self {
        case .notConfigured: "Köprü token'i girilmemiş (Ayarlar)."
        case .unauthorized: "Token yanlış."
        case .unreachable: "Köprüye ulaşılamıyor. Evdeki WiFi'ye bağlı mısın?"
        case .badResponse: "Köprüden beklenmeyen cevap geldi."
        case .setupClosed: "Kurulum penceresi kapalı. Köprüdeki BOOT tuşuna 2 saniye basılı tut, sonra tekrar dene."
        }
    }
}

/// Koprunun yerel HTTP arayuzu (firmware/bridge). Yalnizca ev agindan erisilir.
struct BridgeClient {
    let host: String
    let token: String
    var timeout: TimeInterval = 3

    static func fromConfig() throws -> BridgeClient {
        guard BridgeConfig.isConfigured else { throw BridgeError.notConfigured }
        return BridgeClient(host: BridgeConfig.host, token: BridgeConfig.token)
    }

    private func request(_ path: String, method: String = "GET", query: [URLQueryItem] = [], auth: Bool = true) throws -> URLRequest {
        var c = URLComponents()
        c.scheme = "http"
        let parts = host.split(separator: ":", maxSplits: 1)
        c.host = String(parts.first ?? "")
        if parts.count == 2, let p = Int(parts[1]) { c.port = p }
        c.path = path
        if !query.isEmpty { c.queryItems = query }
        guard let url = c.url else { throw BridgeError.unreachable }
        var r = URLRequest(url: url, timeoutInterval: timeout)
        r.httpMethod = method
        if auth { r.setValue(token, forHTTPHeaderField: "X-Token") }
        return r
    }

    private func send(_ r: URLRequest) async throws -> Data {
        let data: Data, resp: URLResponse
        do { (data, resp) = try await URLSession.shared.data(for: r) }
        catch let e as URLError where e.code == .cancelled { throw CancellationError() }   // iptal edilen istek "ulasilamiyor" degildir
        catch { throw BridgeError.unreachable }
        guard let http = resp as? HTTPURLResponse else { throw BridgeError.badResponse }
        if http.statusCode == 401 { throw BridgeError.unauthorized }
        if http.statusCode == 403 { throw BridgeError.setupClosed }
        guard (200..<300).contains(http.statusCode) else { throw BridgeError.badResponse }
        return data
    }

    struct SetupInfo: Decodable { let name: String; let ip: String; let token: String }

    /// Kopru kurulum penceresi aciksa (WiFi tarayicidan girilince ya da BOOT tusuna 2 sn basilinca, 10 dk) adres ve token'i verir.
    static func fetchSetup(host: String) async throws -> SetupInfo {
        let c = BridgeClient(host: host, token: "", timeout: 4)
        let d = try await c.send(c.request("/api/setup", auth: false))
        guard let i = try? JSONDecoder().decode(SetupInfo.self, from: d), i.name == "nowlight-bridge" else { throw BridgeError.badResponse }
        return i
    }

    /// Token gerekmez: kopru dogru cihaz mi?
    func ping() async throws -> Bool {
        let d = try await send(request("/api/ping", auth: false))
        return String(data: d, encoding: .utf8)?.contains("nowlight-bridge") == true
    }

    func state() async throws -> LightsState {
        let d = try await send(request("/api/state"))
        guard let s = try? JSONDecoder().decode(LightsState.self, from: d) else { throw BridgeError.badResponse }
        return s
    }

    /// Uygulama/widget icin: `.all` yalnizca kaldirilmamis cihazlara tek tek gider.
    func apply(_ light: LightID, on: Bool) async throws {
        if light == .all {
            try await withThrowingTaskGroup(of: Void.self) { g in      // uc istek ayni anda
                for d in LightID.lights { g.addTask { try await self.set(d, on: on) } }
                try await g.waitForAll()
            }
        } else {
            try await set(light, on: on)
        }
    }

    func set(_ light: LightID, on: Bool) async throws {
        _ = try await send(request("/api/set", method: "POST", query: [
            URLQueryItem(name: "dev", value: light.rawValue),
            URLQueryItem(name: "val", value: on ? "1" : "0"),
        ]))
    }

    // MARK: Kablosuz guncelleme (genel kopru)

    struct VersionInfo: Decodable { let version: String; let chip: String; let ota: Bool; let generic: Bool }
    struct UpdateStatus: Decodable {
        let state: String          // idle | checking | downloading | done | error
        let current: String
        let latest: String
        let available: Bool
        let progress: Int
        let error: String
    }

    /// Eski kopru yazilimlari bu adresi bilmez (404 -> badResponse).
    func version() async throws -> VersionInfo {
        let d = try await send(request("/api/version"))
        guard let v = try? JSONDecoder().decode(VersionInfo.self, from: d) else { throw BridgeError.badResponse }
        return v
    }
    private func updateDecode(_ d: Data) throws -> UpdateStatus {
        guard let s = try? JSONDecoder().decode(UpdateStatus.self, from: d) else { throw BridgeError.badResponse }
        return s
    }
    func updateStatus() async throws -> UpdateStatus { try updateDecode(await send(request("/api/update"))) }
    func updateCheck() async throws -> UpdateStatus { try updateDecode(await send(request("/api/update/check", method: "POST"))) }
    func updateStart() async throws -> UpdateStatus { try updateDecode(await send(request("/api/update/start", method: "POST"))) }

    // MARK: Cihaz eslestirme (kopru yazilimi destekliyorsa)

    struct PairCandidate: Decodable, Identifiable, Equatable {
        let mac: String
        let name: String
        let rssi: Int
        let chip: Int?
        let pin: Int?
        let inv: Int?
        var id: String { mac }
        /// Cip adi (kopru bildirir): kullaniciya "ESP32-S3" gibi gosterilir.
        var chipName: String {
            switch chip { case 1: "ESP32"; case 2: "ESP32-S2"; case 3: "ESP32-S3"; case 4: "ESP32-C3"; case 5: "ESP32-C6"; default: "ESP32" }
        }
    }
    struct PairStatus: Decodable {
        let open: Bool
        let secondsLeft: Int
        let found: [PairCandidate]
    }

    private func pairDecode(_ d: Data) throws -> PairStatus {
        guard let s = try? JSONDecoder().decode(PairStatus.self, from: d) else { throw BridgeError.badResponse }
        return s
    }
    /// Eslesme penceresini acar (kopru yeni cihazlari dinler). Eski kopru yazilimi 404 verir -> badResponse.
    func pairStart() async throws -> PairStatus { try pairDecode(await send(request("/api/pair/start", method: "POST"))) }
    func pairStatus() async throws -> PairStatus { try pairDecode(await send(request("/api/pair"))) }
    func pairStop() async { _ = try? await send(request("/api/pair/stop", method: "POST")) }
    /// Cihazi bir isik kimligine baglar: mevcut bir kimlik (yerine gecer) ya da yeni bir kimlik ("d3"...). Ad ve simge uygulamada tutulur.
    func pairAccept(mac: String, id: String, pin: Int? = nil, inv: Bool? = nil, role: NodeRole = .light) async throws {
        var q = [URLQueryItem(name: "mac", value: mac), URLQueryItem(name: "id", value: id), URLQueryItem(name: "role", value: role.rawValue)]
        if let pin { q.append(URLQueryItem(name: "pin", value: String(pin))) }
        if let inv { q.append(URLQueryItem(name: "inv", value: inv ? "1" : "0")) }
        _ = try await send(request("/api/pair/accept", method: "POST", query: q))
    }

    /// Eslesen dugumun role pini ve mantigi (ters: LOW = acik).
    func nodeConfig(id: String, pin: Int, inv: Bool) async throws {
        _ = try await send(request("/api/node/config", method: "POST", query: [
            URLQueryItem(name: "id", value: id), URLQueryItem(name: "pin", value: String(pin)), URLQueryItem(name: "inv", value: inv ? "1" : "0"),
        ]))
    }

    // MARK: Zamanli programlar
    private func scheduleDecode(_ d: Data) throws -> ScheduleInfo {
        guard let i = try? JSONDecoder().decode(ScheduleInfo.self, from: d) else { throw BridgeError.badResponse }
        return i
    }
    /// Kopruda kayitli programlar ve koprunun saati. Eski kopru yazilimi bu adresi bilmez (badResponse).
    func schedules() async throws -> ScheduleInfo { try scheduleDecode(await send(request("/api/schedules"))) }
    /// Programlari kopruye gonderir. Saat dilimi telefondan alinir (UTC'den dakika farki).
    func pushSchedules(_ rules: [ScheduleRule]) async throws -> ScheduleInfo {
        try scheduleDecode(await send(request("/api/schedules", method: "POST", query: [
            URLQueryItem(name: "rules", value: rules.map(\.wire).joined(separator: ";")),
            URLQueryItem(name: "tz", value: String(TimeZone.current.secondsFromGMT() / 60)),
        ])))
    }

    // MARK: Servo ve PIR
    /// Servo ayarlari. hiz: 10 derece/sn biriminde (0 = aninda); birakma: 100 ms biriminde (0 = asla birakma, surekli tut).
    func servoConfig(id: String, pin: Int, inv: Bool, off: Int, on: Int, speed: Int, release: Int, min: Int, max: Int) async throws {
        _ = try await send(request("/api/node/servo", method: "POST", query: [
            URLQueryItem(name: "id", value: id), URLQueryItem(name: "pin", value: String(pin)), URLQueryItem(name: "inv", value: inv ? "1" : "0"),
            URLQueryItem(name: "off", value: String(off)), URLQueryItem(name: "on", value: String(on)), URLQueryItem(name: "speed", value: String(speed)),
            URLQueryItem(name: "release", value: String(release)), URLQueryItem(name: "min", value: String(min)), URLQueryItem(name: "max", value: String(max)),
        ]))
    }
    /// Servoyu bir acyi gonder (0...180). hiz verilmezse servonun ayarli hizi kullanilir.
    func servoGoto(id: String, angle: Int, speed: Int? = nil) async throws {
        var q = [URLQueryItem(name: "id", value: id), URLQueryItem(name: "angle", value: String(angle))]
        if let speed { q.append(URLQueryItem(name: "speed", value: String(speed))) }
        _ = try await send(request("/api/servo", method: "POST", query: q))
    }
    /// Servoyu birak: elle oynatilabilir.
    func servoFree(id: String) async throws {
        _ = try await send(request("/api/servo/free", method: "POST", query: [URLQueryItem(name: "id", value: id)]))
    }
    /// PIR ayarlari: tutma (sn), onay (10 ms), bekleme (sn), ters (LOW = hareket).
    func pirConfig(id: String, pin: Int, hold: Int, confirm: Int, cooldown: Int, inv: Bool) async throws {
        _ = try await send(request("/api/node/pir", method: "POST", query: [
            URLQueryItem(name: "id", value: id), URLQueryItem(name: "pin", value: String(pin)), URLQueryItem(name: "hold", value: String(hold)),
            URLQueryItem(name: "confirm", value: String(confirm)), URLQueryItem(name: "cooldown", value: String(cooldown)), URLQueryItem(name: "inv", value: inv ? "1" : "0"),
        ]))
    }

    /// Dugumun eslesmesini siler; kopru kisaca yeniden baslar.
    func nodeForget(id: String) async throws {
        _ = try await send(request("/api/node/forget", method: "POST", query: [URLQueryItem(name: "id", value: id)]))
    }
}
