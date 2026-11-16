import SwiftUI
import WidgetKit

struct SettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model
    @State private var host = BridgeConfig.host
    @State private var token = BridgeConfig.token
    @State private var result: String?
    @State private var testing = false
    @State private var finding = false
    @State private var ver: BridgeClient.VersionInfo?
    @State private var upd: BridgeClient.UpdateStatus?
    @State private var updMsg: String?
    @State private var updBusy = false
    @State private var findResult: String?

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("nowlight.local", text: $host)
                        .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.URL)
                    SecureField("Token", text: $token)
                        .textInputAutocapitalization(.never).autocorrectionDisabled()
                } header: {
                    Text("Köprü")
                } footer: {
                    Text("Adres: köprünün ev ağındaki adı ya da IP'si (örneğin nowlight.local ya da 192.168.1.50). Token'ı köprünün seri portunda `token` yazarak görürsün.")
                }

                Section {
                    Button {
                        Task { await autoFind() }
                    } label: {
                        HStack { Label("Köprüyü otomatik bul", systemImage: "wand.and.stars"); if finding { Spacer(); ProgressView() } }
                    }
                    .disabled(finding)
                    if let findResult { Text(findResult).font(.footnote).foregroundStyle(findResult.hasPrefix("✓") ? .green : .orange) }
                } footer: {
                    Text("Köprünün WiFi'sini tarayıcıdan kurduktan sonra 10 dakika boyunca, ya da köprüdeki BOOT tuşuna 2 saniye basınca adres ve token kendiliğinden dolar.")
                }

                Section {
                    Button {
                        Task { await test() }
                    } label: {
                        HStack { Text("Bağlantıyı dene"); if testing { Spacer(); ProgressView() } }
                    }
                    .disabled(testing || host.isEmpty)
                    if let result { Text(result).font(.footnote).foregroundStyle(result.hasPrefix("✓") ? .green : .orange) }
                }

                if let ver {
                    Section {
                        HStack { Text("Sürüm"); Spacer(); Text(ver.version).foregroundStyle(.secondary) }
                        if ver.ota {
                            if let u = upd, u.state == "downloading" || u.state == "done" {
                                VStack(alignment: .leading, spacing: 6) {
                                    ProgressView(value: Double(u.progress), total: 100).tint(Color.lampOn)
                                    Text(u.state == "done" ? "Güncelleme yazıldı, köprü yeniden başlıyor…" : "İndiriliyor ve yazılıyor: %\(u.progress)")
                                        .font(.footnote).foregroundStyle(.secondary)
                                }
                            } else if let u = upd, u.available {
                                Button { Task { await startUpdate() } } label: { Label("\(u.latest) sürümüne güncelle", systemImage: "arrow.down.circle.fill") }.disabled(updBusy)
                            } else {
                                Button { Task { await checkUpdate() } } label: {
                                    HStack { Label("Güncellemeleri denetle", systemImage: "arrow.triangle.2.circlepath"); if updBusy { Spacer(); ProgressView() } }
                                }.disabled(updBusy)
                            }
                            if let updMsg { Text(updMsg).font(.footnote).foregroundStyle(updMsg.hasPrefix("✓") ? .green : .orange) }
                        }
                    } header: { Text("Köprü yazılımı") } footer: {
                        Text(ver.ota ? "Köprü yeni yazılımı internetten kendisi indirir. Güncelleme sırasında birkaç dakika ışıkları uygulamadan kontrol edemezsin." : "Bu köprü kablosuz güncellemeyi desteklemiyor (kişisel derleme). USB ile güncellenir.")
                    }
                }

                Section("Widget") {
                    Text("Ana ekranda boş bir yere uzun bas, + düğmesine dokun, \"Nowlight\" widget'ını ekle. Widget düğmeleri uygulamayı açmadan ışığı değiştirir.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Ayarlar")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) { Button("Kaydet") { save(); dismiss() } }
            }
            .task { await loadVersion() }
        }
    }

    private func save() {
        BridgeConfig.host = host.isEmpty ? BridgeConfig.defaultHost : host
        BridgeConfig.token = token
        WidgetCenter.shared.reloadAllTimelines()
        Task { await model.refresh() }
    }

    /// nowlight.local (mDNS) ya da mevcut adres uzerinden kurulum penceresini dener.
    private func autoFind() async {
        finding = true; findResult = nil
        defer { finding = false }
        var lastError: Error?
        for h in [host, BridgeConfig.defaultHost].filter({ !$0.isEmpty }) {
            do {
                let info = try await BridgeClient.fetchSetup(host: h)
                host = info.ip; token = info.token
                findResult = "✓ Köprü bulundu (\(info.ip)). Kaydet'e bas."
                return
            } catch { lastError = error }
        }
        findResult = (lastError as? LocalizedError)?.errorDescription ?? "Köprü bulunamadı. Telefon köprüyle aynı WiFi'de mi?"
    }

    /// Kopru bilgisi: yalnizca baglantisi ve token'i kayitliysa sorulur (eski kopru yazilimi 404 verir -> bolum gizli).
    private func loadVersion() async {
        guard BridgeConfig.isConfigured, let c = try? BridgeClient.fromConfig() else { return }
        ver = try? await c.version()
        if ver?.ota == true { upd = try? await c.updateStatus() }
    }

    private func checkUpdate() async {
        guard let c = try? BridgeClient.fromConfig() else { return }
        updBusy = true; updMsg = nil; defer { updBusy = false }
        do {
            _ = try await c.updateCheck()
            for _ in 0..<20 {                                  // en fazla ~20 sn: kopru sunucuya baglanir
                try await Task.sleep(for: .seconds(1))
                let s = try await c.updateStatus()
                upd = s
                if s.state == "error" { updMsg = s.error; return }
                if s.state != "checking" {
                    updMsg = s.available ? nil : "✓ Köprü güncel (\(s.current))"
                    return
                }
            }
            updMsg = "Denetim uzun sürdü, tekrar dene."
        } catch { updMsg = (error as? LocalizedError)?.errorDescription ?? "Denetlenemedi." }
    }

    private func startUpdate() async {
        guard let c = try? BridgeClient.fromConfig() else { return }
        updBusy = true; updMsg = nil; defer { updBusy = false }
        do {
            upd = try await c.updateStart()
            // ilerlemeyi izle; kopru yazma bitince yeniden baslar (baglanti kopar), sonra yeni surumle geri gelir
            for _ in 0..<240 {
                try await Task.sleep(for: .seconds(1))
                guard let s = try? await c.updateStatus() else {
                    if upd?.state == "done" || (upd?.progress ?? 0) >= 99 { break }       // yeniden basliyor
                    continue
                }
                upd = s
                if s.state == "error" { updMsg = s.error; return }
            }
            await waitForBridge(c)
        } catch { updMsg = (error as? LocalizedError)?.errorDescription ?? "Güncelleme başlatılamadı." }
    }

    private func waitForBridge(_ c: BridgeClient) async {
        updMsg = "Köprü yeniden başlıyor…"
        for _ in 0..<60 {
            try? await Task.sleep(for: .seconds(2))
            if let v = try? await c.version() {
                ver = v; upd = try? await c.updateStatus()
                updMsg = "✓ Güncellendi: sürüm \(v.version)"
                return
            }
        }
        updMsg = "Köprü geri gelmedi. Birkaç dakika bekleyip tekrar kontrol et."
    }

    private func test() async {
        testing = true
        defer { testing = false }
        let client = BridgeClient(host: host.isEmpty ? BridgeConfig.defaultHost : host, token: token.trimmingCharacters(in: .whitespacesAndNewlines))
        do {
            guard try await client.ping() else { result = "Bu adreste köprü yok."; return }
            let s = try await client.state()
            let kanalUyari = s.wifiChannel != s.channel ? " Uyarı: router kanalı \(s.wifiChannel), ESP-NOW kanalı \(s.channel)." : ""
            result = "✓ Köprü bulundu.\(kanalUyari)"
        } catch {
            result = (error as? LocalizedError)?.errorDescription ?? "Bağlantı kurulamadı."
        }
    }
}
