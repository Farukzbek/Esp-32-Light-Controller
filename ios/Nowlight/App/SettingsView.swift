import SwiftUI
import WidgetKit

struct SettingsView: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model
    @State private var host = BridgeConfig.host
    @State private var token = BridgeConfig.token
    @State private var result: String?
    @State private var testing = false

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
                        Task { await test() }
                    } label: {
                        HStack { Text("Bağlantıyı dene"); if testing { Spacer(); ProgressView() } }
                    }
                    .disabled(testing || host.isEmpty)
                    if let result { Text(result).font(.footnote).foregroundStyle(result.hasPrefix("✓") ? .green : .orange) }
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
        }
    }

    private func save() {
        BridgeConfig.host = host.isEmpty ? BridgeConfig.defaultHost : host
        BridgeConfig.token = token
        WidgetCenter.shared.reloadAllTimelines()
        Task { await model.refresh() }
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
