import SwiftUI

/// "Cihaz nasil eklenir?" rehberi: web yukleyici + uygulamadan eslestirme.
struct GuideView: View {
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 22) {
                Text("Yeni bir ışık eklemek üç adım: kartı hazırla, röleyi bağla, uygulamadan eşleştir. Köprüyü yalnızca bir kez kurarsın.")
                    .font(.subheadline).foregroundStyle(.secondary)

                card("1. Köprüyü kur (bir kez)", steps: [
                    "Köprü olarak boştaki bir klasik ESP32 devkit kullan (ışık düğümlerinden ayrı).",
                    "GitHub'daki firmware/bridge yazılımını ESP32'ye yükle (PlatformIO).",
                    "Seri monitörü aç (115200) ve evin WiFi bilgisini gir:",
                ], code: "wifi ağ_adı şifre", more: [
                    "Router'ın 2.4 GHz kanalını sabitle (ESP-NOW ile aynı kanal olmalı).",
                    "Seri monitörde aşağıdakini yaz ve çıkan anahtarı (token) kopyala:",
                ], code2: "token", tail: [
                    "Cihaz Ekle → Köprü ayarları: köprünün IP adresini yaz, anahtarı yapıştır, \"Bağlantıyı dene\"ye bas.",
                ])

                card("2. Kartı hazırla (her yeni ışık için)", steps: [
                    "Bilgisayarda Chrome ya da Edge ile flash.matestudios.online sayfasını aç (iPhone ve Safari olmaz).",
                    "Listeden kendi kartını seç, USB ile tak, \"Yükle\"ye bas. Bir dakika sürer. Port görünmezse BOOT tuşuna basılı tutarak tak.",
                    "Her ışık için bir röle modülü kullan. Sinyal ucunu GPIO5'e (klasik ESP32'de GPIO23), güç uçlarını 5V ve GND'ye bağla.",
                    "Kartı bir USB adaptörüne tak. Eşleşme bekler, başka ayar gerekmez.",
                ])

                card("3. Uygulamadan eşleştir", steps: [
                    "Cihaz Ekle → Cihaz ekle → Cihaz ara. Kartın Nowlight-XXXX adıyla çıkar, üzerine dokun.",
                    "\"Yeni ışık olarak ekle\" de, ad ver, simge seç. Röle ters çalışırsa ya da farklı bir pin kullanıyorsan \"Röle ayarı\"nı aç.",
                    "Bağla'ya bas, sonra Aç/Kapat ile dene.",
                    "Köprü ile aynı WiFi'de olduğundan emin ol. Eşleşme penceresi 2 dakika açık kalır.",
                    "En fazla 7 ışık eklenebilir (3 sabit + 4 yeni).",
                ])

                card("Sorun giderme", steps: [
                    "Cihaz bulunamıyor: kartın fişini çekip tak, telefonu köprüyle aynı WiFi'ye bağla, köprü yazılımının güncel olduğundan emin ol.",
                    "Eşleşmeyi silmek: kartın BOOT tuşuna 5 saniye basılı tut (ya da uygulamada cihazı \"kalıcı sil\").",
                    "Işık ters çalışıyor: cihazı düzenle, \"Röle\" bölümünde ters mantığı değiştir.",
                ])

                VStack(alignment: .leading, spacing: 8) {
                    Label("220V güvenliği", systemImage: "exclamationmark.triangle.fill").font(.subheadline.weight(.semibold)).foregroundStyle(.orange)
                    Text("Röle şebeke gerilimini anahtarlar. Elektrik bilgin yoksa bağlantıyı bir elektrikçiye yaptır, sigortayı kapat, önce yalnızca 5V tarafıyla dene ve 220V bağlantılarını kapalı, yalıtkan bir kutuda yap.")
                        .font(.footnote).foregroundStyle(.white.opacity(0.85))
                }
                .padding(14)
                .background(Color.orange.opacity(0.14), in: RoundedRectangle(cornerRadius: 16, style: .continuous))


                VStack(spacing: 10) {
                    link("Yazılım yükleyici (flash.matestudios.online)", AppInfo.flashURL)
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
