import Foundation

/// Zamanli program: "su saatte, su gunlerde su cihazlari ac / kapat / cevir / servoyu su acaya getir". Kopruda calisir.
/// Telefonda saklanir, kopruye "en:eylem:cihazlar:dakika:gunler:aci;..." olarak gonderilir (bkz. firmware/shared/schedule.h).
struct ScheduleRule: Codable, Identifiable, Equatable {
    enum Action: Int, Codable, CaseIterable {
        case toggle = 0, on = 1, off = 2, angle = 3
        var title: String { switch self { case .toggle: "Çevir"; case .on: "Aç"; case .off: "Kapat"; case .angle: "Açıya git" } }
    }
    var id = UUID()
    var name = "Yeni program"
    var enabled = true
    var action: Action = .on
    var devices = ScheduleRule.defaultMask       // bit = 1 << yuva (masa 0, yatak 1, led 2, eklenenler 3...7)
    var minute = 7 * 60 + 30                     // gun icinde dakika (yerel saat)
    var days = 127                               // bit0 Pazartesi ... bit6 Pazar
    var angle = 90                               // yalniz "Acıya git" icin (servo)

    /// Uygulamada gorunen, ac/kapat yapilabilen cihazlar (isik + servo).
    static var defaultMask: Int { LightID.controllable.reduce(0) { $0 | $1.bit } }
    var effectiveDevices: Int { devices & Self.defaultMask }

    var wire: String {
        let eff = effectiveDevices
        return "\(enabled && eff != 0 ? 1 : 0):\(action.rawValue):\(eff != 0 ? eff : devices):\(minute):\(days & 127):\(angle)"
    }

    static func parse(_ wire: String) -> [ScheduleRule] {
        wire.split(separator: ";").enumerated().compactMap { i, item in
            let v = item.split(separator: ":").compactMap { Int($0) }
            guard v.count == 6, let a = Action(rawValue: v[1]) else { return nil }
            return ScheduleRule(name: "Program \(i + 1)", enabled: v[0] != 0, action: a, devices: v[2], minute: v[3], days: v[4], angle: v[5])
        }
    }

    var timeText: String { String(format: "%02d:%02d", minute / 60, minute % 60) }

    var deviceText: String {
        let shown = LightID.controllable.filter { effectiveDevices & $0.bit != 0 }
        if shown.isEmpty { return "Cihaz yok (kaldırılmış)" }
        return shown.count == LightID.controllable.count && shown.count > 1 ? "Tüm cihazlar" : shown.map(\.title).joined(separator: ", ")
    }

    var actionText: String { action == .angle ? "\(angle)° konumuna git" : action.title }
}

/// Kopruye giden/gelen program bilgisi.
struct ScheduleInfo: Decodable, Equatable {
    let rules: String
    let tz: Int
    let synced: Bool      // kopru saati internetten aldi mi
    let time: String      // kopru yerel saati "HH:MM" ("--:--": alinamadi)
    let count: Int
}
