import AppIntents
import SwiftUI
import WidgetKit

/// Kilit ekrani widget'i (iOS 16+): daire = tek isik dugmesi, dikdortgen = 3 isige kadar, satir ici = ozet.
/// Ayni yapilandirma (Widget'i Duzenle > isiklar) kullanilir.
struct LockLightsView: View {
    @Environment(\.widgetFamily) private var family
    let entry: LightsEntry

    private var lights: [LightID] { entry.lights.filter { $0 != .all } }

    private func button(_ l: LightID) -> some View {
        let on = entry.state?.isOn(l) ?? false
        return Button(intent: ToggleLightIntent(light: l)) {
            ZStack {
                AccessoryWidgetBackground()
                Image(systemName: l.symbol).font(.system(size: 18, weight: .semibold))
                    .symbolVariant(on ? .fill : .none)
                    .opacity(on ? 1 : 0.55)
            }
        }
        .buttonStyle(.plain)
        .accessibilityLabel("\(l.title), \(on ? "açık" : "kapalı")")
    }

    var body: some View {
        switch family {
        case .accessoryCircular:
            if let l = lights.first { button(l) } else { Image(systemName: "lightbulb.slash") }
        case .accessoryRectangular:
            VStack(alignment: .leading, spacing: 4) {
                HStack(spacing: 6) { ForEach(lights.prefix(3), id: \.self) { button($0).frame(width: 40, height: 40) } }
                Text(entry.state == nil ? "Bağlantı yok" : "\(entry.state?.onCount ?? 0) ışık açık").font(.caption2).lineLimit(1)
            }
        case .accessoryInline:
            Label(entry.state == nil ? "Nowlight: bağlantı yok" : "\(entry.state?.onCount ?? 0) ışık açık", systemImage: "lightbulb.fill")
        default:
            EmptyView()
        }
    }
}

struct LockScreenLightsWidget: Widget {
    var body: some WidgetConfiguration {
        AppIntentConfiguration(kind: "LockScreenLights", intent: LightsConfigurationIntent.self, provider: LightsProvider()) { entry in
            LockLightsView(entry: entry).containerBackground(.clear, for: .widget)
        }
        .configurationDisplayName("Işıklar (kilit ekranı)")
        .description("Kilit ekranından ışıkları aç/kapat. Widget'ı düzenleyerek hangi ışığın görüneceğini seç.")
        .supportedFamilies([.accessoryCircular, .accessoryRectangular, .accessoryInline])
    }
}
