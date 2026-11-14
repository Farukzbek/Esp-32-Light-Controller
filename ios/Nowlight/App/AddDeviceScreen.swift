import SwiftUI

/// Cihaz Ekle sekmesi. Otomatik cihaz arama/kurulum sonraki surumde gelecek; cihaz yonetimi (kaldir / geri ekle) simdiden var.
struct AddDeviceScreen: View {
    @Environment(AppModel.self) private var model
    @Binding var showSettings: Bool
    @State private var pendingRemove: LightID?

    private var active: [LightID] { _ = model.removed; return LightID.devices }
    private var removedList: [LightID] { LightID.allDevices.filter { model.removed.contains($0.rawValue) } }

    var body: some View {
        NavigationStack {
            List {
                Section {
                    VStack(alignment: .leading, spacing: 14) {
                        HStack(spacing: 14) {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                                .font(.system(size: 24, weight: .semibold)).foregroundStyle(.black)
                                .frame(width: 52, height: 52).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Yeni cihaz ekle").font(.headline)
                                Text("Yakında").font(.caption.weight(.semibold)).foregroundStyle(.black)
                                    .padding(.horizontal, 8).padding(.vertical, 2).background(Color.lampOn, in: Capsule())
                            }
                        }
                        Text("Kodu yüklenmiş, henüz hiçbir köprüye bağlanmamış bir ESP32'yi ev ağında kendisi bulacak ve kurulumu otomatik yapacak. Kod ya da adres girmen gerekmeyecek.")
                            .font(.subheadline).foregroundStyle(.secondary)
                        Button {} label: { Label("Cihaz ara", systemImage: "magnifyingglass").frame(maxWidth: .infinity) }
                            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).disabled(true)
                    }
                    .padding(.vertical, 6)
                }

                Section {
                    NavigationLink { GuideView() } label: {
                        Label("Cihaz nasıl eklenir?", systemImage: "book.fill")
                    }
                } footer: { Text("Otomatik kurulum gelene kadar adım adım rehber.") }

                Section {
                    if active.isEmpty { Text("Gösterilen cihaz yok.").foregroundStyle(.secondary) }
                    ForEach(active, id: \.self) { light in
                        DeviceRow(light: light)
                            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                                Button(role: .destructive) { pendingRemove = light } label: { Label("Kaldır", systemImage: "trash") }
                            }
                    }
                } header: { Text("Cihazlarım") } footer: {
                    if !active.isEmpty { Text("Bir cihazı kaldırmak için sola kaydır.") }
                }

                if !removedList.isEmpty {
                    Section {
                        ForEach(removedList, id: \.self) { light in
                            HStack {
                                Label(light.title, systemImage: light.symbol).foregroundStyle(.secondary)
                                Spacer()
                                Button("Geri ekle") { model.restore(light) }.buttonStyle(.bordered).tint(Color.lampOn)
                            }
                        }
                    } header: { Text("Kaldırılanlar") }
                }

                Section("Köprü") {
                    Button { showSettings = true } label: {
                        Label("Köprü ayarları (adres ve token)", systemImage: "wifi.router.fill")
                    }
                }

                Section {
                    Link(destination: AppInfo.repoURL) {
                        HStack {
                            Label("Açık kaynak: GitHub", systemImage: "chevron.left.forwardslash.chevron.right")
                            Spacer()
                            Image(systemName: "arrow.up.right").font(.footnote.weight(.semibold)).foregroundStyle(.secondary)
                        }
                    }
                    HStack {
                        Text("Sürüm").foregroundStyle(.secondary)
                        Spacer()
                        Text(AppInfo.version).foregroundStyle(.secondary)
                    }
                } header: { Text("Hakkında") } footer: {
                    Text("Nowlight açık kaynaklı bir projedir: ESP32 yazılımı, köprü ve bu uygulamanın kodu GitHub'da.")
                }
            }
            .scrollContentBackground(.hidden)
            .background(Color.black.ignoresSafeArea())
            .navigationTitle("Cihaz Ekle")
            .confirmationDialog(
                pendingRemove.map { "\($0.title) kaldırılsın mı?" } ?? "",
                isPresented: Binding(get: { pendingRemove != nil }, set: { if !$0 { pendingRemove = nil } }),
                titleVisibility: .visible
            ) {
                Button("Kaldır", role: .destructive) { if let l = pendingRemove { model.remove(l) }; pendingRemove = nil }
                Button("Vazgeç", role: .cancel) { pendingRemove = nil }
            } message: {
                Text("Cihaz yalnızca uygulamadan ve widget'lardan kaldırılır, ESP32 çalışmaya devam eder. Kaldırılanlar listesinden geri ekleyebilirsin.")
            }
        }
    }
}

struct DeviceRow: View {
    @Environment(AppModel.self) private var model
    let light: LightID

    private var linked: Bool? {
        guard let s = model.state else { return nil }
        switch light {
        case .masa, .led: return s.links.masa
        case .yatak: return s.links.yatak
        case .all: return nil
        }
    }

    var body: some View {
        HStack(spacing: 14) {
            Image(systemName: light.symbol).font(.system(size: 18, weight: .semibold)).foregroundStyle(.black)
                .frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
            VStack(alignment: .leading, spacing: 2) {
                Text(light.title).font(.body.weight(.semibold))
                HStack(spacing: 6) {
                    Circle().fill(linked == true ? Color.barOn : (linked == false ? Color.orange : Color.gray)).frame(width: 7, height: 7)
                    Text(linked == true ? "Bağlı" : (linked == false ? "Ulaşılamıyor" : "—")).font(.caption).foregroundStyle(.secondary)
                }
            }
        }
        .padding(.vertical, 2)
    }
}
