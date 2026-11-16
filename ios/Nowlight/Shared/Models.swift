import AppIntents
import SwiftUI

/// Kontrol edilen isiklar. `all` = hepsi. Uc yerlesik isik (masa, yatak, led) ve kullanicinin ekledigi isiklar ("d3"..."d7", en fazla 8 isik).
struct LightID: Hashable, Identifiable {
    let rawValue: String
    var id: String { rawValue }

    private init(raw: String) { rawValue = raw }
    static let masa = LightID(raw: "masa")
    static let yatak = LightID(raw: "yatak")
    static let led = LightID(raw: "led")
    static let all = LightID(raw: "all")

    /// Yalnizca bilinen bir isik kimligiyle olusur.
    init?(rawValue: String) {
        guard rawValue == "all" || Self.allDevices.contains(where: { $0.rawValue == rawValue }) else { return nil }
        self.rawValue = rawValue
    }

    static let maxDevices = 8
    /// Kopruyle ortak "yuva" numarasi: masa 0, yatak 1, led 2, eklenenler 3...7. Otomasyon bit maskesi de buna gore (1 << yuva).
    var slot: Int? {
        switch rawValue {
        case "masa": 0
        case "yatak": 1
        case "led": 2
        case "all": nil
        default: rawValue.hasPrefix("d") ? Int(rawValue.dropFirst()) : nil
        }
    }
    var bit: Int { slot.map { 1 << $0 } ?? 0 }
    var isBuiltin: Bool { rawValue == "masa" || rawValue == "yatak" || rawValue == "led" }

    /// Tum fiziksel cihazlar (kaldirilanlar dahil), yuva sirasiyla
    static var allDevices: [LightID] {
        let custom = BridgeConfig.customDevices.map { LightID(raw: $0) }.sorted { ($0.slot ?? 99) < ($1.slot ?? 99) }
        return [.masa, .yatak, .led] + custom
    }
    /// Uygulamada gorunen (kaldirilmamis) cihazlar
    static var devices: [LightID] { allDevices.filter { !BridgeConfig.removed.contains($0.rawValue) } }

    /// Yeni eklenecek isik icin bos yuvanin kimligi (yoksa nil: 8 isik dolu)
    static func nextCustom() -> LightID? {
        let used = Set(allDevices.compactMap(\.slot))
        return (3..<maxDevices).first { !used.contains($0) }.map { LightID(raw: "d\($0)") }
    }

    /// Kullanicinin verdigi ad (yoksa nil). "Tum isiklar" yeniden adlandirilamaz.
    var customName: String? {
        guard self != .all, let n = BridgeConfig.names[rawValue]?.trimmingCharacters(in: .whitespaces), !n.isEmpty else { return nil }
        return n
    }

    var title: String { customName ?? defaultTitle }

    var defaultTitle: String {
        switch rawValue {
        case "masa": "Masa lambası"
        case "yatak": "Yatak lambası"
        case "led": "LED şerit"
        case "all": "Tüm ışıklar"
        default: "Yeni ışık"
        }
    }

    var shortTitle: String {
        if let n = customName { return n }
        switch rawValue {
        case "masa": return "Masa"
        case "yatak": return "Yatak"
        case "led": return "LED"
        case "all": return "Tümü"
        default: return "Işık"
        }
    }

    /// Secilen simge (varsa); "Tum isiklar" degistirilemez.
    var customSymbol: String? { self == .all ? nil : BridgeConfig.icons[rawValue] }
    var symbol: String { customSymbol ?? defaultSymbol }

    var defaultSymbol: String {
        switch rawValue {
        case "masa": "lamp.desk.fill"
        case "yatak": "bed.double.fill"
        case "led": "light.ribbon.fill"
        case "all": "lightswitch.on.fill"
        default: "lightbulb.fill"
        }
    }
}

/// Cihaz icin secilebilen hazir SF Symbols simgeleri (emoji degil, Apple simgeleri).
enum IconChoices {
    static let all: [String] = [
        "lightbulb.fill", "lightbulb.led.fill", "lightbulb.2.fill", "lamp.desk.fill", "lamp.table.fill", "lamp.floor.fill",
        "lamp.ceiling.fill", "chandelier.fill", "light.cylindrical.ceiling.fill", "light.recessed.fill", "light.panel.fill", "light.ribbon.fill",
        "bed.double.fill", "sofa.fill", "tv.fill", "desktopcomputer", "book.fill", "gamecontroller.fill",
        "fork.knife", "bathtub.fill", "door.left.hand.open", "window.shade.open", "fan.fill", "flame.fill",
        "moon.stars.fill", "sun.max.fill", "sparkles", "star.fill", "bolt.fill", "house.fill",
        "leaf.fill", "music.note", "party.popper.fill", "powerplug.fill",
    ]
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

/// Koprunun bildirdigi durum. Deger: 1 acik, 0 kapali, -1 henuz bilinmiyor. Anahtar: isik kimligi.
/// Kopru eski uc alani (masa, yatak, led) ve `devices` sozlugunu (eklenen isiklar) yollar; `links` baglanti durumudur.
struct LightsState: Codable, Equatable {
    var values: [String: Int]
    var links: [String: Bool]
    var channel: Int
    var wifiChannel: Int

    init(values: [String: Int], links: [String: Bool], channel: Int, wifiChannel: Int) {
        self.values = values; self.links = links; self.channel = channel; self.wifiChannel = wifiChannel
    }

    enum CodingKeys: String, CodingKey { case masa, yatak, led, devices, links, channel, wifiChannel }

    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        var v = (try c.decodeIfPresent([String: Int].self, forKey: .devices)) ?? [:]
        v["masa"] = try c.decode(Int.self, forKey: .masa)
        v["yatak"] = try c.decode(Int.self, forKey: .yatak)
        v["led"] = try c.decode(Int.self, forKey: .led)
        values = v
        links = (try? c.decode([String: Bool].self, forKey: .links)) ?? [:]
        channel = try c.decode(Int.self, forKey: .channel)
        wifiChannel = try c.decode(Int.self, forKey: .wifiChannel)
    }

    func encode(to encoder: Encoder) throws {
        var c = encoder.container(keyedBy: CodingKeys.self)
        try c.encode(values["masa"] ?? -1, forKey: .masa)
        try c.encode(values["yatak"] ?? -1, forKey: .yatak)
        try c.encode(values["led"] ?? -1, forKey: .led)
        try c.encode(values.filter { !["masa", "yatak", "led"].contains($0.key) }, forKey: .devices)
        try c.encode(links, forKey: .links)
        try c.encode(channel, forKey: .channel)
        try c.encode(wifiChannel, forKey: .wifiChannel)
    }

    func value(_ light: LightID) -> Int {
        light == .all ? (LightID.devices.contains { value($0) == 1 } ? 1 : 0) : (values[light.rawValue] ?? -1)
    }

    func isOn(_ light: LightID) -> Bool { value(light) == 1 }

    /// Dugum baglanti durumu. LED, masa dugumune bagli. Kopru bilmiyorsa (anahtar yok) sorun yok sayilir.
    func linked(_ light: LightID) -> Bool { links[light == .led ? "masa" : light.rawValue] ?? true }

    /// "Tum isiklar" icin acik sayisi
    var onCount: Int { LightID.devices.filter { isOn($0) }.count }
    /// Hepsi acik mi / bir kismi mi acik (kart sadece hepsi aciksa tamamen yanar)
    var allOn: Bool { onCount == LightID.devices.count }
    var partial: Bool { onCount > 0 && !allOn }

    func setting(_ light: LightID, on: Bool) -> LightsState {
        var s = self
        if light == .all { for d in LightID.devices { s.values[d.rawValue] = on ? 1 : 0 } }
        else { s.values[light.rawValue] = on ? 1 : 0 }
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
