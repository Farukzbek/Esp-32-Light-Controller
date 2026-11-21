import Foundation

/// Koprunun ya da kapi dugumunun olay gunlugu kaydi. t: saniye (epoch; 0 = cihazin saati yoktu).
struct LogEvent: Decodable, Identifiable, Hashable {
    let t: Int
    let s: String
    var id: String { "\(t)|\(s)" }
}

/// Gecmis ekraninda ve bildirimlerde gosterilen olay.
struct HistoryItem: Identifiable, Hashable {
    enum Kind: String, CaseIterable { case program, timer, door, command
        var title: String { switch self { case .program: "Program"; case .timer: "Zamanlayıcı"; case .door: "Kapı otomasyonu"; case .command: "Komut" } }
        var symbol: String { switch self { case .program: "calendar.badge.clock"; case .timer: "timer"; case .door: "figure.walk.motion"; case .command: "hand.tap" } }
    }
    let id: String
    let date: Date?
    let kind: Kind
    let text: String

    /// Cihazin ham metnini okunur Turkce'ye cevirir ("ok: masa=1 yatak=0" -> "masa açıldı, yatak kapandı").
    static func pretty(_ raw: String) -> String {
        // "kapat (cihazlar 0x7)" / "acildi (cihazlar 0x3)" / "aci 90 (cihazlar 0x8)" bicimi: cihaz adlariyla okunur cumle
        if let r = raw.range(of: #"\(cihazlar 0x([0-9a-f]+)\)"#, options: .regularExpression) {
            let hex = raw[r].dropFirst("(cihazlar 0x".count).dropLast()
            let mask = Int(hex, radix: 16) ?? 0
            let names = LightID.allDevices.filter { mask & $0.bit != 0 }.map(\.title)
            let who = names.isEmpty ? "Cihazlar" : (names.count == LightID.allDevices.count && names.count > 1 ? "Tüm cihazlar" : names.joined(separator: ", "))
            let head = raw[..<r.lowerBound].trimmingCharacters(in: .whitespaces)
            var what = head
            for p in ["program: ", "zamanlayici: "] where what.hasPrefix(p) { what = String(what.dropFirst(p.count)) }
            let verb: String
            if what.hasPrefix("aci ") { verb = "\(what.dropFirst(4))° konumuna gitti" }
            else { verb = ["ac": "açıldı", "acildi": "açıldı", "kapat": "kapandı", "kapatildi": "kapandı", "cevir": "çevrildi"][what] ?? what }
            return "\(who) \(verb)"
        }
        var t = raw
        var lead = ""
        if let r = t.range(of: #"^kural \d+ "#, options: .regularExpression) { t = String(t[r.upperBound...]) }       // "kural 2 ok: ..." -> "ok: ..."
        if t.hasPrefix("gir-cik, ters: ") { lead = "Gir-çık: "; t = String(t.dropFirst("gir-cik, ters: ".count)) }
        else if t.hasPrefix("cikis (isik acikti), kapat: ") { lead = "Çıkış: "; t = String(t.dropFirst("cikis (isik acikti), kapat: ".count)) }
        if t.hasPrefix("zamanlayici kuruldu: ") { return "Zamanlayıcı kuruldu: " + t.dropFirst("zamanlayici kuruldu: ".count).replacingOccurrences(of: " sonra ac", with: " sonra aç").replacingOccurrences(of: " sonra kapat", with: " sonra kapat") }
        for p in ["program: ", "zamanlayici: ", "zamanlayici kuruldu: ", "komut: ", "ok: ", "test: "] where t.hasPrefix(p) { t = String(t.dropFirst(p.count)) }
        // "masa=1 yatak=0" bicimi
        let pairs = t.split(separator: " ").compactMap { w -> String? in
            let kv = w.split(separator: "="); guard kv.count == 2, let v = Int(kv[1]) else { return nil }
            let name = LightID(rawValue: String(kv[0]))?.shortTitle ?? String(kv[0])
            return "\(name) \(v == 1 ? "açıldı" : "kapandı")"
        }
        return lead + (pairs.isEmpty ? t : pairs.joined(separator: ", "))
    }

    static func from(bridge e: LogEvent) -> HistoryItem {
        let kind: Kind = e.s.hasPrefix("program") ? .program : (e.s.hasPrefix("zamanlayici") ? .timer : .command)
        return HistoryItem(id: "b|" + e.id, date: e.t > 0 ? Date(timeIntervalSince1970: TimeInterval(e.t)) : nil, kind: kind, text: pretty(e.s))
    }
    static func from(door e: LogEvent) -> HistoryItem {
        HistoryItem(id: "d|" + e.id, date: e.t > 0 ? Date(timeIntervalSince1970: TimeInterval(e.t)) : nil, kind: .door, text: pretty(e.s))
    }
}
