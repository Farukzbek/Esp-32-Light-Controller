import Foundation

enum BridgeError: LocalizedError {
    case notConfigured, unauthorized, unreachable, badResponse

    var errorDescription: String? {
        switch self {
        case .notConfigured: "Köprü token'i girilmemiş (Ayarlar)."
        case .unauthorized: "Token yanlış."
        case .unreachable: "Köprüye ulaşılamıyor. Evdeki WiFi'ye bağlı mısın?"
        case .badResponse: "Köprüden beklenmeyen cevap geldi."
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
        guard (200..<300).contains(http.statusCode) else { throw BridgeError.badResponse }
        return data
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
                for d in LightID.devices { g.addTask { try await self.set(d, on: on) } }
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
}
