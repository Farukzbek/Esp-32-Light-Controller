import AppIntents
import SwiftUI
import WidgetKit

struct LightsProvider: AppIntentTimelineProvider {
    func placeholder(in context: Context) -> LightsEntry { .sample() }

    func snapshot(for configuration: LightsConfigurationIntent, in context: Context) async -> LightsEntry {
        if context.isPreview { return .sample(lights: configuration.lights, accent: configuration.accent) }
        return await load(configuration)
    }

    func timeline(for configuration: LightsConfigurationIntent, in context: Context) async -> Timeline<LightsEntry> {
        // Dugmeye yeni basildiysa (iyimser durum) agi beklemeden hemen goster, 2 sn sonra gercek durumu oku.
        if let c = StateCache.load(), c.optimistic, Date().timeIntervalSince(c.at) < 4 {
            let e = LightsEntry(date: .now, state: c.state, stale: false, fetchedAt: c.at, error: nil, lights: configuration.lights, accent: configuration.accent)
            return Timeline(entries: [e], policy: .after(.now.addingTimeInterval(2)))
        }
        // iOS yenileme sikligini kendisi belirler; yine de 15 dakikada bir iste. Dugmeye basinca ayrica hemen yenilenir.
        return Timeline(entries: [await load(configuration)], policy: .after(.now.addingTimeInterval(15 * 60)))
    }

    private func load(_ c: LightsConfigurationIntent) async -> LightsEntry {
        do {
            let s = try await BridgeClient.fromConfig().state()
            StateCache.save(s)
            return LightsEntry(date: .now, state: s, stale: false, fetchedAt: .now, error: nil, lights: c.lights, accent: c.accent)
        } catch {
            let msg = (error as? LocalizedError)?.errorDescription ?? "Bağlantı yok"
            if let cached = StateCache.load() {
                // Guncel durum bilinmiyor: eski "acik" bilgisini gostermek yaniltir, o yuzden hepsi kapali gorunur (soluk).
                var off = cached.state; off.masa = 0; off.yatak = 0; off.led = 0
                return LightsEntry(date: .now, state: off, stale: true, fetchedAt: cached.at, error: msg, lights: c.lights, accent: c.accent)
            }
            return LightsEntry(date: .now, state: nil, stale: false, fetchedAt: nil, error: msg, lights: c.lights, accent: c.accent)
        }
    }
}

struct LightsWidget: Widget {
    var body: some WidgetConfiguration {
        AppIntentConfiguration(kind: "LightsWidget", intent: LightsConfigurationIntent.self, provider: LightsProvider()) { entry in
            LightsWidgetView(entry: entry)
                .containerBackground(Color.black, for: .widget)
        }
        .configurationDisplayName("Işıklar")
        .description("Masa, yatak ve LED şerit ışıklarını açıp kapat. Widget'ı düzenleyerek hangi ışıkların ve hangi rengin görüneceğini seç. Evdeki WiFi'de çalışır.")
        .supportedFamilies([.systemSmall, .systemMedium, .systemLarge])
    }
}

@main
struct NowlightWidgets: WidgetBundle {
    var body: some Widget { LightsWidget() }
}
