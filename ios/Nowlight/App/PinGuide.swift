import SwiftUI

/// Pin rehberi: hangi işlemcide hangi pin ne için kullanılır. Eşleşmede köprü işlemci türünü bildirir (chip: 1 ESP32, 2 S2, 3 S3, 4 C3, 5 C6).
/// Genel bilgidir (Espressif belgeleri); kartının şemasına göre kontrol et: üretici bazı pinleri ayrıca kullanmış olabilir (dahili LED, USB, PSRAM).
enum PinLevel: Int, Comparable {
    case good, inputOnly, caution, avoid
    static func < (a: PinLevel, b: PinLevel) -> Bool { a.rawValue < b.rawValue }
    var color: Color { switch self { case .good: .green; case .inputOnly: .blue; case .caution: .orange; case .avoid: .red } }
    var icon: String { switch self { case .good: "checkmark.circle.fill"; case .inputOnly: "arrow.down.circle.fill"; case .caution: "exclamationmark.triangle.fill"; case .avoid: "xmark.octagon.fill" } }
    var title: String { switch self { case .good: "Önerilen"; case .inputOnly: "Yalnızca giriş"; case .caution: "Dikkat"; case .avoid: "Kullanma" } }
}

struct PinGuide {
    struct Entry { let pins: [Int]; let level: PinLevel; let note: String }
    let chip: Int
    let name: String
    let entries: [Entry]
    let defaultPin: Int          // yazilimin varsayilan role pini

    static func chipName(_ chip: Int?) -> String {
        switch chip { case 1: "ESP32"; case 2: "ESP32-S2"; case 3: "ESP32-S3"; case 4: "ESP32-C3"; case 5: "ESP32-C6"; default: "ESP32" }
    }

    static func guide(for chip: Int?) -> PinGuide? {
        guard let chip, (1...5).contains(chip) else { return nil }
        return all[chip - 1]
    }

    /// Bu pin bu rol icin uygun mu? Rol cikis ister (isik/servo) ya da giris (PIR).
    func check(pin: Int, role: NodeRole) -> (level: PinLevel, text: String) {
        guard let e = entries.first(where: { $0.pins.contains(pin) }) else {
            return (.caution, "GPIO\(pin) bu işlemcide yok ya da rehberde tanımlı değil. Kartının pin şemasına bak.")
        }
        if e.level == .inputOnly { return role == .pir ? (.good, "GPIO\(pin): \(e.note) PIR için uygun.") : (.avoid, "GPIO\(pin): \(e.note) \(role == .servo ? "Servo" : "Röle") için çıkış gerekir.") }
        return (e.level, "GPIO\(pin): \(e.note)")
    }

    /// Role gore onerilen varsayilan pin: cihazin bildirdigi role pini (isik, servo), PIR icin giris icin guvenli bir pin.
    static func defaultPin(for role: NodeRole, chip: Int?, reported: Int?) -> Int {
        if role == .pir { return chip == 5 ? 6 : 4 }
        return reported ?? guide(for: chip)?.defaultPin ?? 5
    }

    /// "1, 2, 4–18, 21" gibi kisa gosterim
    static func ranges(_ pins: [Int]) -> String {
        let p = Array(Set(pins)).sorted()
        var out: [String] = [], i = 0
        while i < p.count {
            var j = i
            while j + 1 < p.count && p[j + 1] == p[j] + 1 { j += 1 }
            out.append(j - i >= 2 ? "\(p[i])–\(p[j])" : p[i...j].map(String.init).joined(separator: ", "))
            i = j + 1
        }
        return out.joined(separator: ", ")
    }

    static let roleAdvice: [NodeRole: String] = [
        .light: "Röle: çıkış pini. Açılışta röle kendiliğinden çekmesin diye strapping (açılış ayarı) pinlerinden kaçın.",
        .servo: "Servo sinyali: PWM üretebilen herhangi bir çıkış pini. Servoyu kartın USB'sinden değil, ayrı bir 5V kaynaktan besle ve GND'leri birleştir.",
        .pir: "PIR: giriş pini (3,3V sinyal). Modülü 5V ile besle. Klasik ESP32'de yalnızca-giriş pinleri (34, 35, 36, 39) PIR için idealdir.",
    ]

    static let all: [PinGuide] = [
        PinGuide(chip: 1, name: "ESP32 (klasik)", entries: [
            Entry(pins: [4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33], level: .good, note: "Serbest kullanılır."),
            Entry(pins: [34, 35, 36, 39], level: .inputOnly, note: "Yalnızca giriş (çıkış yok, dahili çekme direnci yok)."),
            Entry(pins: [5], level: .caution, note: "Açılışta kısa süre PWM sinyali verir (strapping); çoğu röle modülünde sorun olmaz."),
            Entry(pins: [2], level: .caution, note: "Strapping, çoğu kartta dahili LED; açılışta serbest ya da LOW olmalı."),
            Entry(pins: [12], level: .caution, note: "Strapping: açılışta LOW olmalı. Röle bunu HIGH çekerse kart açılmayabilir."),
            Entry(pins: [15], level: .caution, note: "Strapping: açılışta sinyal verir."),
            Entry(pins: [0], level: .avoid, note: "BOOT tuşu (strapping)."),
            Entry(pins: [1, 3], level: .avoid, note: "USB-seri TX/RX: yükleme ve günlük için kullanılır."),
            Entry(pins: [6, 7, 8, 9, 10, 11], level: .avoid, note: "Dahili flaş belleğe bağlı: kullanılamaz."),
        ], defaultPin: 23),
        PinGuide(chip: 2, name: "ESP32-S2", entries: [
            Entry(pins: Array(1...18) + [21], level: .good, note: "Serbest kullanılır."),
            Entry(pins: Array(33...37), level: .caution, note: "PSRAM'li modülde (WROVER) meşgul olabilir; S2 Mini'de serbest."),
            Entry(pins: [38, 39, 40], level: .good, note: "Serbest kullanılır."),
            Entry(pins: [46], level: .inputOnly, note: "Yalnızca giriş (strapping)."),
            Entry(pins: [0], level: .avoid, note: "BOOT tuşu (strapping)."),
            Entry(pins: [19, 20], level: .avoid, note: "USB D-/D+: yerleşik USB için kullanılır."),
            Entry(pins: Array(26...32), level: .avoid, note: "Dahili flaş/PSRAM'e bağlı."),
            Entry(pins: [43, 44], level: .avoid, note: "UART0 TX/RX."),
            Entry(pins: [45], level: .avoid, note: "Strapping (flaş voltajı)."),
        ], defaultPin: 5),
        PinGuide(chip: 3, name: "ESP32-S3", entries: [
            Entry(pins: [1, 2] + Array(4...18) + [21, 38, 39, 40, 41, 42, 47], level: .good, note: "Serbest kullanılır."),
            Entry(pins: [3], level: .caution, note: "Strapping (JTAG kaynağı); çıkış olarak çoğunlukla sorunsuz."),
            Entry(pins: [48], level: .caution, note: "Bazı kartlarda dahili RGB LED."),
            Entry(pins: [46], level: .inputOnly, note: "Yalnızca giriş (strapping)."),
            Entry(pins: [0], level: .avoid, note: "BOOT tuşu (strapping)."),
            Entry(pins: [19, 20], level: .avoid, note: "USB D-/D+: yerleşik USB bu yazılımda kullanılıyor."),
            Entry(pins: Array(26...32), level: .avoid, note: "Dahili flaş/PSRAM'e bağlı."),
            Entry(pins: Array(33...37), level: .avoid, note: "Octal PSRAM'li modüllerde (N8R8, N16R8) meşgul."),
            Entry(pins: [43, 44], level: .avoid, note: "UART0 TX/RX."),
            Entry(pins: [45], level: .avoid, note: "Strapping (flaş voltajı)."),
        ], defaultPin: 5),
        PinGuide(chip: 4, name: "ESP32-C3", entries: [
            Entry(pins: [0, 1, 3, 4, 5, 6, 7, 10], level: .good, note: "Serbest kullanılır."),
            Entry(pins: [2], level: .caution, note: "Strapping: açılışta HIGH olmalı."),
            Entry(pins: [8], level: .caution, note: "Strapping; çoğu kartta (Super Mini) dahili LED."),
            Entry(pins: [9], level: .caution, note: "BOOT tuşu (strapping)."),
            Entry(pins: [18, 19], level: .avoid, note: "USB D-/D+: yerleşik USB bu yazılımda kullanılıyor."),
            Entry(pins: [20, 21], level: .avoid, note: "UART0 RX/TX."),
            Entry(pins: Array(11...17), level: .avoid, note: "Dahili flaşa bağlı."),
        ], defaultPin: 5),
        PinGuide(chip: 5, name: "ESP32-C6", entries: [
            Entry(pins: [0, 1, 2, 3, 6, 7, 10, 11, 14, 18, 19, 20, 21, 22, 23], level: .good, note: "Serbest kullanılır."),
            Entry(pins: [4, 5, 8, 15], level: .caution, note: "Strapping (açılış ayarı): röle çekili kalırsa açılışı bozabilir; varsayılan GPIO5 çoğu rölede çalışır."),
            Entry(pins: [9], level: .caution, note: "BOOT tuşu (strapping)."),
            Entry(pins: [12, 13], level: .avoid, note: "USB D-/D+: yerleşik USB bu yazılımda kullanılıyor."),
            Entry(pins: [16, 17], level: .avoid, note: "UART0 TX/RX."),
            Entry(pins: Array(24...30), level: .avoid, note: "Dahili flaşa bağlı."),
        ], defaultPin: 5),
    ]
}

/// Secili pinin durumu (renkli tek satir)
struct PinHint: View {
    let chip: Int?
    let role: NodeRole
    let pin: Int
    var body: some View {
        if let g = PinGuide.guide(for: chip) {
            let r = g.check(pin: pin, role: role)
            Label(r.text, systemImage: r.level.icon).font(.footnote).foregroundStyle(r.level.color)
        } else {
            Text("İşlemci türü bilinmiyor; pin rehberi için cihazı eşleştirirken seçilen işlemciye bakılır.").font(.footnote).foregroundStyle(.secondary)
        }
    }
}

/// Isleme gore tum pinlerin rehberi: onerilen, dikkat, kullanma.
struct PinGuideView: View {
    let chip: Int?
    let role: NodeRole

    var body: some View {
        if let g = PinGuide.guide(for: chip) {
            VStack(alignment: .leading, spacing: 10) {
                Text(PinGuide.roleAdvice[role] ?? "").font(.footnote).foregroundStyle(.secondary)
                ForEach([PinLevel.good, .inputOnly, .caution, .avoid], id: \.self) { level in
                    let items = g.entries.filter { $0.level == level }
                    if !items.isEmpty {
                        VStack(alignment: .leading, spacing: 4) {
                            Label("\(level.title): " + PinGuide.ranges(items.flatMap(\.pins)), systemImage: level.icon)
                                .font(.footnote.weight(.semibold)).foregroundStyle(level.color)
                            if level != .good {
                                ForEach(items.indices, id: \.self) { i in
                                    Text("• GPIO" + PinGuide.ranges(items[i].pins) + ": " + items[i].note).font(.caption).foregroundStyle(.secondary)
                                }
                            }
                        }
                    }
                }
                Text("Genel bilgidir; kartının pin şemasına bak (dahili LED, USB, PSRAM gibi üretici seçimleri değişebilir). \(role == .pir ? "PIR için önerilen: GPIO\(PinGuide.defaultPin(for: .pir, chip: chip, reported: nil))." : "Varsayılan pin: GPIO\(g.defaultPin).")")
                    .font(.caption2).foregroundStyle(.tertiary)
            }
        }
    }
}
