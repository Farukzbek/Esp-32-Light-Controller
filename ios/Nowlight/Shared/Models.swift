import AppIntents
import SwiftUI

/// Kontrol edilen isiklar. `all` = hepsi.
enum LightID: String, CaseIterable, AppEnum {
    case masa, yatak, led, all

    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Işık" }
    static var caseDisplayRepresentations: [LightID: DisplayRepresentation] {
        [.masa: "Masa lambası", .yatak: "Yatak lambası", .led: "LED şerit", .all: "Tüm ışıklar"]
    }

    var title: String {
        switch self {
        case .masa: "Masa lambası"
        case .yatak: "Yatak lambası"
        case .led: "LED şerit"
        case .all: "Tüm ışıklar"
        }
    }

    var shortTitle: String {
        switch self {
        case .masa: "Masa"
        case .yatak: "Yatak"
        case .led: "LED"
        case .all: "Tümü"
        }
    }

    /// Tum fiziksel cihazlar (kaldirilanlar dahil)
    static var allDevices: [LightID] { [.masa, .yatak, .led] }
    /// Uygulamada gorunen (kaldirilmamis) cihazlar
    static var devices: [LightID] { allDevices.filter { !BridgeConfig.removed.contains($0.rawValue) } }

    var symbol: String {
        switch self {
        case .masa: "lamp.desk.fill"
        case .yatak: "bed.double.fill"
        case .led: "light.ribbon.fill"
        case .all: "lightswitch.on.fill"
        }
    }
}

/// Widget ayarinda bir yuva icin secenek (bos = o yuva gosterilmez).
enum SlotChoice: String, CaseIterable, AppEnum {
    case masa, yatak, led, all, bos

    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Işık" }
    static var caseDisplayRepresentations: [SlotChoice: DisplayRepresentation] {
        [.masa: "Masa lambası", .yatak: "Yatak lambası", .led: "LED şerit", .all: "Tüm ışıklar", .bos: "Boş"]
    }

    var light: LightID? { LightID(rawValue: rawValue) }
}

/// Widget'ta "acik" ışığın rengi.
enum AccentChoice: String, CaseIterable, AppEnum {
    case amber, green, blue, white

    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Renk" }
    static var caseDisplayRepresentations: [AccentChoice: DisplayRepresentation] {
        [.amber: "Sarı", .green: "Yeşil", .blue: "Mavi", .white: "Beyaz"]
    }

    var color: Color {
        switch self {
        case .amber: .lampOn
        case .green: Color(hex: 0x34D27B)
        case .blue: Color(hex: 0x4C8DFF)
        case .white: Color(hex: 0xF2F2F2)
        }
    }

    /// Acik karonun uzerindeki yazi/simge rengi
    var ink: Color { self == .blue ? .white : Color(hex: 0x14120C) }
}

/// Koprunun bildirdigi durum. Deger: 1 acik, 0 kapali, -1 henuz bilinmiyor.
struct LightsState: Codable, Equatable {
    struct Links: Codable, Equatable { var masa: Bool; var yatak: Bool }
    var masa: Int
    var yatak: Int
    var led: Int
    var links: Links
    var channel: Int
    var wifiChannel: Int

    func value(_ light: LightID) -> Int {
        switch light {
        case .masa: masa
        case .yatak: yatak
        case .led: led
        case .all: LightID.devices.contains { value($0) == 1 } ? 1 : 0
        }
    }

    func isOn(_ light: LightID) -> Bool { value(light) == 1 }

    /// "Tum isiklar" icin acik sayisi
    var onCount: Int { LightID.devices.filter { isOn($0) }.count }
    /// Hepsi acik mi / bir kismi mi acik (kart sadece hepsi aciksa tamamen yanar)
    var allOn: Bool { onCount == LightID.devices.count }
    var partial: Bool { onCount > 0 && !allOn }

    func setting(_ light: LightID, on: Bool) -> LightsState {
        var s = self
        let v = on ? 1 : 0
        switch light {
        case .masa: s.masa = v
        case .yatak: s.yatak = v
        case .led: s.led = v
        case .all: for d in LightID.devices { s = s.setting(d, on: on) }
        }
        return s
    }
}

extension Color {
    init(hex: UInt32) {
        self.init(red: Double((hex >> 16) & 0xFF) / 255, green: Double((hex >> 8) & 0xFF) / 255, blue: Double(hex & 0xFF) / 255)
    }
    /// Panel (CYD) ile ayni renkler
    static let lampOn = Color(hex: 0xFFC93C)
    static let lampOff = Color(hex: 0x2A2A2A)
    static let barOn = Color(hex: 0x00C853)
    static let barOff = Color(hex: 0xE53935)
}
