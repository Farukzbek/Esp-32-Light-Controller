import SwiftUI

/// Ilk kurulum: kopruyu otomatik bulup adres ve token'i kaydeder (kopru kurulum penceresi aciksa). Ayarlar, Cihaz ekle ve ana ekran kullanir.
enum BridgeSetup {
    /// nil = basarili ve kaydedildi; aksi halde kullaniciya gosterilecek hata metni.
    @MainActor static func autoFindAndSave(model: AppModel) async -> String? {
        var lastError: Error?
        for h in [BridgeConfig.host, BridgeConfig.defaultHost].filter({ !$0.isEmpty }) {
            do {
                let info = try await BridgeClient.fetchSetup(host: h)
                BridgeConfig.host = info.ip; BridgeConfig.token = info.token
                model.reloadConfig()
                return nil
            } catch { lastError = error }
        }
        return (lastError as? LocalizedError)?.errorDescription ?? "Köprü bulunamadı. Telefon köprüyle aynı WiFi'de mi?"
    }
}

/// Kopru henuz taninmadiysa gosterilen kart: tek dokunusla otomatik bul.
struct BridgeConnectCard: View {
    @Environment(AppModel.self) private var model
    @State private var busy = false
    @State private var message: String?
    var onConnected: () -> Void = {}

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Label("Önce köprüyü bağla", systemImage: "wifi.router.fill").font(.headline)
            Text("Köprü, telefonunu ışıklara bağlayan küçük ESP32'dir. Köprünün WiFi'sini tarayıcıdan kurduktan sonra 10 dakika boyunca (ya da köprüdeki BOOT tuşuna 2 saniye basınca) uygulama onu kendisi bulur.")
                .font(.footnote).foregroundStyle(.secondary)
            Button {
                Task {
                    busy = true; defer { busy = false }
                    message = await BridgeSetup.autoFindAndSave(model: model)
                    if message == nil { message = "✓ Köprü bulundu ve kaydedildi."; onConnected() }
                }
            } label: { HStack { if busy { ProgressView().tint(.black) }; Label("Köprüyü otomatik bul", systemImage: "wand.and.stars") }.frame(maxWidth: .infinity) }
                .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large).disabled(busy)
            if let message { Text(message).font(.footnote).foregroundStyle(message.hasPrefix("✓") ? .green : .orange) }
            Text("Adresi ve token'ı elle girmek için Ayarlar'a bak (Cihaz Ekle sekmesi → Köprü ayarları).").font(.caption).foregroundStyle(.tertiary)
        }
        .padding(16)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(Color.lampOn.opacity(0.10), in: RoundedRectangle(cornerRadius: 18, style: .continuous))
    }
}
