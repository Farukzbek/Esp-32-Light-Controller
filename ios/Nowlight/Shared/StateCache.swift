import Foundation

/// Son basarili durumu saklar: koprüye ulasilamazsa widget bunu "guncel degil" uyarisiyla gosterir.
enum StateCache {
    private static var defaults: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }

    static func save(_ s: LightsState) {
        guard let d = try? JSONEncoder().encode(s) else { return }
        defaults.set(d, forKey: "cachedState")
        defaults.set(Date(), forKey: "cachedAt")
    }

    static func load() -> (state: LightsState, at: Date)? {
        guard let d = defaults.data(forKey: "cachedState"), let s = try? JSONDecoder().decode(LightsState.self, from: d),
              let at = defaults.object(forKey: "cachedAt") as? Date else { return nil }
        return (s, at)
    }
}
