import AppIntents
import SwiftUI
import WidgetKit

struct LightsEntry: TimelineEntry {
    let date: Date
    let state: LightsState?
    let stale: Bool            // durum onbellekten: kopruye ulasilamadi
    let fetchedAt: Date?
    let error: String?
    let lights: [LightID]
    let accent: AccentChoice
}

extension LightsEntry {
    static func sample(lights: [LightID] = [.masa, .yatak, .led, .all], accent: AccentChoice = .amber, error: String? = nil, stale: Bool = false) -> LightsEntry {
        LightsEntry(date: .now, state: LightsState(values: ["masa": 1, "yatak": 0, "led": 0], links: ["masa": true, "yatak": true], channel: 1, wifiChannel: 1),
                    stale: stale, fetchedAt: .now, error: error, lights: lights, accent: accent)
    }
}

enum TileSize { case compact, regular, large }

/// Karonun gorunumu (dugme ve Toggle ortak). `on`: yanik mi; `partial`: "Tum isiklar" icin bir kismi acik.
struct TileFace: View {
    let light: LightID
    let entry: LightsEntry
    let size: TileSize
    let on: Bool
    var partial = false

    private var value: Int { entry.state?.value(light) ?? -1 }
    private var ink: Color { on ? entry.accent.ink : (partial ? entry.accent.color : .white) }

    private var status: String {
        guard let s = entry.state else { return "—" }
        if light == .all { return s.onCount == 0 ? "Kapalı" : "\(s.onCount)/\(LightID.devices.count)" + (s.allOn ? " açık" : "") }
        return value == -1 ? "—" : (on ? "Açık" : "Kapalı")
    }

    var body: some View {
        Group {
            switch size {
            case .compact:
                VStack(spacing: 3) {
                    Image(systemName: light.symbol).font(.system(size: 20, weight: .semibold))
                    Text(light.shortTitle).font(.system(size: 11, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.7)
                }
            case .regular:
                VStack(spacing: 4) {
                    Image(systemName: light.symbol).font(.system(size: 26, weight: .semibold))
                    Text(light.shortTitle).font(.system(size: 13, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.7)
                    Text(status).font(.system(size: 10, weight: .medium)).opacity(0.75)
                }
            case .large:
                VStack(alignment: .leading, spacing: 0) {
                    Image(systemName: light.symbol).font(.system(size: 30, weight: .semibold))
                    Spacer(minLength: 4)
                    Text(light.title).font(.system(size: 17, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.7)
                    Text(status).font(.system(size: 13, weight: .medium)).opacity(0.75)
                }
                .padding(14)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
            }
        }
        .foregroundStyle(ink)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(on ? entry.accent.color : Color.lampOff)
        .clipShape(RoundedRectangle(cornerRadius: size == .large ? 20 : 14, style: .continuous))
        .opacity(entry.state == nil ? 0.45 : (entry.stale ? 0.75 : 1))
    }
}

/// Toggle'in gorunumu: dokunulunca sistem `isOn`'u HEMEN cevirir, komut arkada calisirken karo aninda degisir.
struct TileToggleStyle: ToggleStyle {
    let light: LightID
    let entry: LightsEntry
    let size: TileSize

    func makeBody(configuration: Configuration) -> some View {
        TileFace(light: light, entry: entry, size: size, on: configuration.isOn)
    }
}

/// Tek bir ışığın dokunulabilir karosu (iOS 17: uygulamayı açmadan ışığı değiştirir).
/// Hedef durum cizim aninda degil, komut calisirken en guncel (iyimser) duruma gore hesaplanir (ToggleLightIntent):
/// bu sayede hizli art arda basislar dogru calisir.
struct WidgetLightButton: View {
    let light: LightID
    let entry: LightsEntry
    let size: TileSize

    private var value: Int { entry.state?.value(light) ?? -1 }

    var body: some View {
        if light == .all {
            // "Tum isiklar": kismi durum gosterimi var, sistem Toggle'i yerine duz dugme
            Button(intent: ToggleLightIntent(light: .all)) {
                TileFace(light: .all, entry: entry, size: size, on: entry.state?.allOn ?? false, partial: entry.state?.partial ?? false)
            }
            .buttonStyle(.plain)
        } else {
            Toggle(isOn: value == 1, intent: ToggleLightIntent(light: light)) { Text(light.title) }
                .toggleStyle(TileToggleStyle(light: light, entry: entry, size: size))
        }
    }
}

/// Karolari 2 sutunlu dizer; tek kalan son karo iki sutunu kaplar.
private struct TileGrid: View {
    let entry: LightsEntry
    let size: TileSize
    let spacing: CGFloat

    var body: some View {
        let l = entry.lights
        Grid(horizontalSpacing: spacing, verticalSpacing: spacing) {
            ForEach(0..<((l.count + 1) / 2), id: \.self) { row in
                GridRow {
                    if row * 2 + 1 < l.count {
                        WidgetLightButton(light: l[row * 2], entry: entry, size: size)
                        WidgetLightButton(light: l[row * 2 + 1], entry: entry, size: size)
                    } else {
                        WidgetLightButton(light: l[row * 2], entry: entry, size: size).gridCellColumns(2)
                    }
                }
            }
        }
    }
}

/// Widget icinde ortamdaki boyutu kullanir.
struct LightsWidgetView: View {
    @Environment(\.widgetFamily) private var family
    let entry: LightsEntry

    var body: some View { LightsWidgetContent(entry: entry, family: family) }
}

/// Boyut parametre olarak verilir (uygulamadaki onizleme de ayni gorunumu kullanabilsin diye).
struct LightsWidgetContent: View {
    let entry: LightsEntry
    let family: WidgetFamily

    private var statusLine: (text: String, color: Color) {
        if entry.state == nil { return (entry.error ?? "Köprüye ulaşılamıyor", .orange) }
        if entry.stale, let t = entry.fetchedAt { return ("Güncel değil · " + t.formatted(date: .omitted, time: .shortened), .orange) }
        let n = entry.state?.onCount ?? 0
        return (n == 0 ? "Hepsi kapalı" : "\(n) ışık açık", .secondary)
    }

    var body: some View {
        Group {
            if entry.lights.isEmpty {
                Text("Widget'ı düzenleyip ışık seç").font(.footnote).foregroundStyle(.secondary)
            } else {
                switch family {
                case .systemSmall:
                    TileGrid(entry: entry, size: .compact, spacing: 6)
                case .systemLarge:
                    VStack(spacing: 10) {
                        HStack {
                            Text("Işıklar").font(.system(size: 17, weight: .bold)).foregroundStyle(.white)
                            Spacer()
                            Circle().fill(statusLine.color == .secondary ? Color.barOn : Color.orange).frame(width: 7, height: 7)
                            Text(statusLine.text).font(.system(size: 12)).foregroundStyle(statusLine.color).lineLimit(1)
                        }
                        TileGrid(entry: entry, size: .large, spacing: 8)
                        HStack(spacing: 8) {
                            Button(intent: SetAllLightsIntent(on: true)) { Label("Hepsini aç", systemImage: "sun.max.fill").frame(maxWidth: .infinity) }
                            Button(intent: SetAllLightsIntent(on: false)) { Label("Hepsini kapat", systemImage: "moon.fill").frame(maxWidth: .infinity) }
                        }
                        .font(.system(size: 13, weight: .semibold))
                        .buttonStyle(.plain)
                        .foregroundStyle(.white)
                        .labelStyle(.titleAndIcon)
                        .padding(.vertical, 10)
                        .background(Color.white.opacity(0.1), in: Capsule())
                    }
                default:
                    VStack(spacing: 6) {
                        HStack(spacing: 8) {
                            ForEach(entry.lights, id: \.self) { WidgetLightButton(light: $0, entry: entry, size: .regular) }
                        }
                        Text(statusLine.text).font(.system(size: 10)).foregroundStyle(statusLine.color).lineLimit(1)
                    }
                }
            }
        }
    }
}
