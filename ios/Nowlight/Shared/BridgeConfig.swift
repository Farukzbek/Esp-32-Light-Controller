import Foundation

/// Kopru adresi ve token'i. Uygulama ve widget ayni App Group'tan okur.
/// Gelistirme: uygulamayi `-bridgeHost 127.0.0.1:8080 -bridgeToken test` baslatma argumanlariyla acarsan onlar kullanilir.
enum BridgeConfig {
    static let group = "group.online.matestudios.nowlight"
    static let defaultHost = "nowlight.local"

    private static var shared: UserDefaults { UserDefaults(suiteName: group) ?? .standard }

    static var host: String {
        get { UserDefaults.standard.string(forKey: "bridgeHost") ?? shared.string(forKey: "host") ?? defaultHost }
        set { shared.set(newValue.trimmingCharacters(in: .whitespaces), forKey: "host") }
    }

    static var token: String {
        get { UserDefaults.standard.string(forKey: "bridgeToken") ?? shared.string(forKey: "token") ?? "" }
        set { shared.set(newValue.trimmingCharacters(in: .whitespacesAndNewlines), forKey: "token") }
    }

    static var isConfigured: Bool { !token.isEmpty }

    /// Uygulamadan kaldirilan cihazlar (LightID.rawValue). Yalnizca uygulamada gizlenir; ESP32 calismaya devam eder, "Cihaz Ekle"den geri eklenir.
    static var removed: Set<String> {
        get { Set(shared.stringArray(forKey: "removedDevices") ?? []) }
        set { shared.set(Array(newValue).sorted(), forKey: "removedDevices") }
    }
}
