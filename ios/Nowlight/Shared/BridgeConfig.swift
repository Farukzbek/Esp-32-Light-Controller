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

    /// Kullanicinin verdigi cihaz adlari (LightID.rawValue -> ad). Bos/yok ise varsayilan ad kullanilir.
    static var names: [String: String] {
        get { (shared.dictionary(forKey: "deviceNames") as? [String: String]) ?? [:] }
        set { shared.set(newValue, forKey: "deviceNames") }
    }

    /// Kullanicinin sectigi SF Symbols simgeleri (LightID.rawValue -> simge adi).
    static var icons: [String: String] {
        get { (shared.dictionary(forKey: "deviceIcons") as? [String: String]) ?? [:] }
        set { shared.set(newValue, forKey: "deviceIcons") }
    }

    /// Kullanicinin ekledigi isiklarin kimlikleri ("d3"..."d7"). Yerlesik uc isik (masa, yatak, led) burada tutulmaz.
    static var customDevices: [String] {
        get { shared.stringArray(forKey: "customDevices") ?? [] }
        set { shared.set(newValue, forKey: "customDevices") }
    }

    /// Eslesen cihazlarin rolu (LightID.rawValue -> "light" | "servo" | "pir"). Kopru bildirir; yoksa isik.
    static var roles: [String: String] {
        get { (shared.dictionary(forKey: "deviceRoles") as? [String: String]) ?? [:] }
        set { shared.set(newValue, forKey: "deviceRoles") }
    }

    /// Servo cihazlarinin telefondaki ayar kopyasi (kimlik -> "pin:ters:kapali:acik:hiz:birakma:min:max"), PIR icin ("pin:tutma:onay:bekleme:ters").
    static var roleSettings: [String: String] {
        get { (shared.dictionary(forKey: "roleSettings") as? [String: String]) ?? [:] }
        set { shared.set(newValue, forKey: "roleSettings") }
    }

    /// Eklenen isiklarin role ayari (LightID.rawValue -> "pin:ters", ters 1/0). Dugumde saklanan degerin telefondaki kopyasi.
    static var relay: [String: String] {
        get { (shared.dictionary(forKey: "relayConfig") as? [String: String]) ?? [:] }
        set { shared.set(newValue, forKey: "relayConfig") }
    }

    /// Kopru "eski/kisisel" derleme mi (sabit masa, yatak, led dugumleri var)? Genel koprude false: yerlesik isiklar gizlenir, her sey eslesmeyle eklenir.
    static var legacy: Bool {
        get { (shared.object(forKey: "bridgeLegacy") as? Bool) ?? true }
        set { shared.set(newValue, forKey: "bridgeLegacy") }
    }
}
