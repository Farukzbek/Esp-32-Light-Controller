import SwiftUI

/// "ESP32 nasil eklenir?" rehberi. Otomatik kurulum gelene kadar gecerli (elle) adimlar.
struct GuideView: View {
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                Text("Otomatik cihaz kurulumu yakında geliyor. Şimdilik iki adım var: önce köprüyü bir kez kur, sonra ışık düğümlerini ekle.")
                    .font(.subheadline).foregroundStyle(.secondary)

                card("1. Köprüyü kur (bir kez)", steps: [
                    "Köprü olarak boştaki bir klasik ESP32 devkit kullan (ışık düğümlerinden ayrı).",
                    "GitHub'daki firmware/bridge yazılımını ESP32'ye yükle.",
                    "Seri monitörü aç (115200) ve evin WiFi bilgisini gir:",
                ], code: "wifi ağ_adı şifre", more: [
                    "Router'ın 2.4 GHz kanalını 1'e sabitle (ESP-NOW ile aynı kanal olmalı).",
                    "Seri monitörde aşağıdakini yaz ve çıkan anahtarı (token) kopyala:",
                ], code2: "token", tail: [
                    "Cihaz Ekle → Köprü ayarları: adrese nowlight.local yaz, anahtarı yapıştır, \"Bağlantıyı dene\"ye bas.",
                ])

                card("2. Işık düğümü ekle (şimdilik elle)", steps: [
                    "Her ışık için bir ESP32 ve bir röle modülü kullan. Röle ters mantıklıysa (LOW = açık) kod buna göre hazır.",
                    "ESP32'nin MAC adresini oku, firmware/shared/now_config.h dosyasına ekle. ESP-NOW kanalı ve parola tüm cihazlarda aynı olmalı.",
                    "Düğüm yazılımını yükle (örnek: firmware/bed-node).",
                    "Köprünün MAC adresini NOW_BRIDGE_MAC olarak düğüm ayarına yaz ve düğümü yeniden yükle.",
                    "Düğüm bağlanınca Cihaz Ekle → Cihazlarım listesinde \"Bağlı\" görünür.",
                ])

                VStack(alignment: .leading, spacing: 8) {
                    Label("220V güvenliği", systemImage: "exclamationmark.triangle.fill").font(.subheadline.weight(.semibold)).foregroundStyle(.orange)
                    Text("Röle şebeke gerilimini anahtarlar. Elektrik bilgin yoksa bağlantıyı bir elektrikçiye yaptır, sigortayı kapat, önce yalnızca 5V tarafıyla dene ve 220V bağlantılarını kapalı, yalıtkan bir kutuda yap.")
                        .font(.footnote).foregroundStyle(.white.opacity(0.85))
                }
                .padding(14)
                .background(Color.orange.opacity(0.14), in: RoundedRectangle(cornerRadius: 16, style: .continuous))

                Text("Yeni bir ışık türü eklemek (örneğin dördüncü bir ışık) için uygulamanın da güncellenmesi gerekir. Bu, otomatik kurulumla gelecek.")
                    .font(.footnote).foregroundStyle(.secondary)

                VStack(spacing: 10) {
                    link("Ayrıntılı kurulum rehberi", AppInfo.docsURL)
                    link("GitHub'da projeyi aç", AppInfo.repoURL)
                }
            }
            .padding(18)
        }
        .scrollIndicators(.hidden)
        .background(Color.black.ignoresSafeArea())
        .navigationTitle("Cihaz nasıl eklenir?")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func card(_ title: String, steps: [String], code: String? = nil, more: [String] = [], code2: String? = nil, tail: [String] = []) -> some View {
        var items: [AnyView] = []
        for s in steps { items.append(AnyView(stepText(s))) }
        if let c = code { items.append(AnyView(codeBox(c))) }
        for s in more { items.append(AnyView(stepText(s))) }
        if let c = code2 { items.append(AnyView(codeBox(c))) }
        for s in tail { items.append(AnyView(stepText(s))) }
        return VStack(alignment: .leading, spacing: 12) {
            Text(title).font(.headline)
            ForEach(items.indices, id: \.self) { items[$0] }
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 20, style: .continuous))
    }

    private func stepText(_ t: String) -> some View {
        HStack(alignment: .top, spacing: 10) {
            Circle().fill(Color.lampOn).frame(width: 6, height: 6).padding(.top, 7)
            Text(t).font(.subheadline).foregroundStyle(.white.opacity(0.88))
        }
    }

    private func codeBox(_ t: String) -> some View {
        Text(t).font(.system(.footnote, design: .monospaced)).foregroundStyle(Color.lampOn)
            .padding(.horizontal, 12).padding(.vertical, 8)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.black.opacity(0.6), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            .padding(.leading, 16)
    }

    private func link(_ title: String, _ url: URL) -> some View {
        Link(destination: url) {
            HStack {
                Text(title).font(.subheadline.weight(.semibold))
                Spacer()
                Image(systemName: "arrow.up.right")
            }
            .padding(14)
            .background(Color.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        }
        .foregroundStyle(.white)
    }
}
