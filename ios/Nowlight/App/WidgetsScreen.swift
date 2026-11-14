import SwiftUI
import WidgetKit

/// Widgetlar sekmesi: uc boyutun canli onizlemesi ve ana ekrana nasil eklenecegi.
struct WidgetsScreen: View {
    @Environment(AppModel.self) private var model

    private var entry: LightsEntry {
        _ = model.removed                                  // kaldirma degisince onizleme tazelensin
        let d = LightID.devices
        let lights = d.count >= 2 ? d + [.all] : d
        if let s = model.state {
            return LightsEntry(date: .now, state: s, stale: false, fetchedAt: model.lastUpdate, error: nil, lights: lights, accent: .amber)
        }
        return .sample(lights: lights)
    }

    private func preview(_ family: WidgetFamily, w: CGFloat, h: CGFloat) -> some View {
        LightsWidgetContent(entry: entry, family: family)
            .padding(family == .systemLarge ? 16 : 14)
            .frame(width: w, height: h)
            .background(Color.black)
            .clipShape(RoundedRectangle(cornerRadius: 28, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 28, style: .continuous).stroke(Color.white.opacity(0.12)))
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 22) {
                    Text("Ana ekranına ekleyebileceğin üç boyut. Önizlemeler gerçek durumunu gösterir.")
                        .font(.subheadline).foregroundStyle(.secondary)

                    section("Küçük") { preview(.systemSmall, w: 160, h: 160) }
                    section("Orta") { preview(.systemMedium, w: 338, h: 160) }
                    section("Büyük") { preview(.systemLarge, w: 338, h: 354) }

                    VStack(alignment: .leading, spacing: 12) {
                        Text("Nasıl eklerim?").font(.headline)
                        step("1", "Ana ekranda boş bir yere uzun bas.")
                        step("2", "Sol üstteki + düğmesine dokun, \"Nowlight\"ı seç.")
                        step("3", "Boyutu seçip \"Widget Ekle\"ye dokun.")
                        step("4", "Düzenlemek için widget'a uzun bas, \"Widget'ı Düzenle\": hangi ışıkların görüneceğini (en fazla 4, sırasıyla) ve açık rengini seç.")
                    }
                    .padding(16)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(Color.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 20, style: .continuous))
                }
                .padding(18)
            }
            .scrollIndicators(.hidden)
            .background(Color.black.ignoresSafeArea())
            .navigationTitle("Widgetlar")
        }
    }

    private func section<C: View>(_ title: String, @ViewBuilder content: () -> C) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title).font(.footnote.weight(.semibold)).foregroundStyle(.secondary).textCase(.uppercase)
            content()
        }
    }

    private func step(_ n: String, _ text: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Text(n).font(.footnote.weight(.bold)).foregroundStyle(.black).frame(width: 22, height: 22).background(Color.lampOn, in: Circle())
            Text(text).font(.subheadline).foregroundStyle(.white.opacity(0.85))
        }
    }
}
