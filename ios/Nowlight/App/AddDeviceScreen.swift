import SwiftUI

/// Cihaz Ekle sekmesi. Otomatik cihaz arama/kurulum sonraki surumde gelecek; cihaz yonetimi (kaldir / geri ekle) simdiden var.
struct AddDeviceScreen: View {
    @Environment(AppModel.self) private var model
    @Binding var showSettings: Bool
    @State private var pendingRemove: LightID?
    @State private var renaming: LightID?
    @State private var showPairing = false

    private var active: [LightID] { _ = model.removed; return LightID.devices }
    private var removedList: [LightID] { LightID.allDevices.filter { model.removed.contains($0.rawValue) } }

    var body: some View {
        let _ = (model.names, model.icons)
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
                            }
                        }
                        Text("Nowlight yazılımı yüklü bir ESP32'yi fişe tak, uygulama onu bulsun. Rolünü seç, birkaç adımda ışığın hazır.")
                            .font(.subheadline).foregroundStyle(.secondary)
                        Button { showPairing = true } label: { Label("Cihaz ekle", systemImage: "magnifyingglass").frame(maxWidth: .infinity) }
                            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black)
                    }
                    .padding(.vertical, 6)
                }

                Section {
                    NavigationLink { GuideView() } label: {
                        Label("Cihaz nasıl eklenir?", systemImage: "book.fill")
                    }
                } footer: { Text("Yazılım yükleme ve kurulum için adım adım rehber.") }

                Section {
                    if active.isEmpty { Text("Gösterilen cihaz yok.").foregroundStyle(.secondary) }
                    ForEach(active, id: \.self) { light in
                        DeviceRow(light: light)
                            .contentShape(Rectangle())
                            .onTapGesture { renaming = light }
                            .swipeActions(edge: .leading, allowsFullSwipe: false) {
                                Button { renaming = light } label: { Label("Düzenle", systemImage: "pencil") }.tint(Color.lampOn)
                            }
                            .swipeActions(edge: .trailing, allowsFullSwipe: false) {
                                Button(role: .destructive) { pendingRemove = light } label: { Label("Kaldır", systemImage: "trash") }
                            }
                    }
                } header: { Text("Cihazlarım") } footer: {
                    if !active.isEmpty { Text("Adını ve simgesini değiştirmek için cihaza dokun, kaldırmak için sola kaydır.") }
                }

                Section {
                    NavigationLink { AutomationScreen() } label: {
                        Label("Kapı otomasyonu", systemImage: "figure.walk.motion")
                    }
                } header: { Text("Otomasyon") } footer: { Text("Kapının üstündeki sensör odaya girişi algılayınca seçtiğin ışıkları kendisi yakar.") }

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

                Section {
                    NavigationLink { RemovedDevicesScreen() } label: {
                        HStack {
                            Label("Kaldırılanlar", systemImage: "arrow.uturn.backward.circle")
                            Spacer()
                            if !removedList.isEmpty { Text("\(removedList.count)").foregroundStyle(.secondary) }
                        }
                    }
                }
            }
            .scrollContentBackground(.hidden)
            .background(Color.black.ignoresSafeArea())
            .navigationTitle("Cihaz Ekle")
            .sheet(item: $renaming) { light in DeviceEditor(light: light) }
            .sheet(isPresented: $showPairing) { PairingFlow() }
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
        guard let s = model.state, light != .all else { return nil }
        return s.linked(light)
    }

    var body: some View {
        let _ = (model.names, model.icons)
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

/// Cihazin adini ve simgesini degistirme penceresi.
struct DeviceEditor: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model
    let light: LightID
    @State private var name: String
    @State private var symbol: String
    @State private var relayPin: Int
    @State private var relayInv: Bool
    @State private var relayMsg: String?
    @State private var busy = false
    @State private var confirmDelete = false

    init(light: LightID) {
        self.light = light
        _name = State(initialValue: light.customName ?? "")
        _symbol = State(initialValue: light.symbol)
        _relayPin = State(initialValue: light.relayConfig?.pin ?? 5)
        _relayInv = State(initialValue: light.relayConfig?.inv ?? true)
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    HStack(spacing: 14) {
                        Image(systemName: symbol).font(.system(size: 26, weight: .semibold)).foregroundStyle(.black)
                            .frame(width: 56, height: 56).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 15, style: .continuous))
                        TextField(light.defaultTitle, text: $name).font(.title3.weight(.semibold))
                    }
                } header: { Text("Ad") } footer: { Text("En fazla 20 karakter. Uygulamada, widget'larda ve Siri'de görünür.") }

                Section("Simge") {
                    LazyVGrid(columns: Array(repeating: GridItem(.flexible(), spacing: 10), count: 6), spacing: 10) {
                        ForEach(IconChoices.all, id: \.self) { s in
                            Button { symbol = s } label: {
                                Image(systemName: s).font(.system(size: 20, weight: .semibold))
                                    .foregroundStyle(symbol == s ? .black : .white)
                                    .frame(maxWidth: .infinity).frame(height: 44)
                                    .background(symbol == s ? Color.lampOn : Color.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                            }.buttonStyle(.plain)
                        }
                    }.padding(.vertical, 4)
                }

                if !light.isBuiltin {
                    Section {
                        Stepper(value: $relayPin, in: 0...48) { HStack { Text("Röle pini"); Spacer(); Text("GPIO\(relayPin)").foregroundStyle(.secondary) } }
                        Toggle("Ters mantık (LOW = açık)", isOn: $relayInv).tint(Color.lampOn)
                        Button {
                            Task {
                                busy = true; defer { busy = false }
                                do { try await model.setRelay(light, pin: relayPin, inv: relayInv); relayMsg = "✓ Cihaza gönderildi." }
                                catch { relayMsg = (error as? LocalizedError)?.errorDescription ?? "Gönderilemedi." }
                            }
                        } label: { HStack { Text("Cihaza uygula"); if busy { Spacer(); ProgressView() } } }.disabled(busy)
                        if let relayMsg { Text(relayMsg).font(.footnote).foregroundStyle(relayMsg.hasPrefix("✓") ? .green : .orange) }
                    } header: { Text("Röle") } footer: { Text("Işık ters çalışıyorsa mantığı değiştir. Pin değişince cihaz yeni pini kullanır; röleyi yeni pine taşıman gerekir.") }

                    Section {
                        Button("Cihazı kalıcı sil", role: .destructive) { confirmDelete = true }
                    } footer: { Text("Eşleşmeyi köprüden ve cihazdan siler. Cihaz yeniden eşleşmeye hazır olur. Köprü birkaç saniye yeniden başlar.") }
                }

                if light.isBuiltin && light != .all {
                    Section {
                        Label("Röle pini ve mantığı", systemImage: "info.circle").foregroundStyle(.secondary)
                    } footer: { Text("Röle ayarı yalnızca uygulamadan eşleştirilerek eklenen yeni ışıklarda görünür (Cihaz Ekle → Cihaz ekle). Masa, yatak ve LED düğümlerinin ayarı yazılımlarında sabittir.") }
                }

                Section {
                    Button("Varsayılana dön", role: .destructive) {
                        model.rename(light, to: ""); model.setIcon(light, nil); dismiss()
                    }
                }
            }
            .navigationTitle(light.defaultTitle)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Vazgeç") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Kaydet") { model.rename(light, to: name); model.setIcon(light, symbol); dismiss() }
                }
            }
        }
        .presentationDetents([.large])
        .confirmationDialog("\(light.title) kalıcı silinsin mi?", isPresented: $confirmDelete, titleVisibility: .visible) {
            Button("Kalıcı sil", role: .destructive) {
                Task {
                    do { try await model.deleteDevice(light); dismiss() }
                    catch { relayMsg = (error as? LocalizedError)?.errorDescription ?? "Silinemedi. Köprüye ulaşılıyor mu?" }
                }
            }
            Button("Vazgeç", role: .cancel) {}
        } message: { Text("Cihaz köprüyle bağlantısını kaybeder ve yeniden eşleşmesi gerekir. Otomasyonlardan da kalkar.") }
    }
}

/// Kaldirilan cihazlar: buradan geri eklenir.
struct RemovedDevicesScreen: View {
    @Environment(AppModel.self) private var model
    private var removedList: [LightID] { LightID.allDevices.filter { model.removed.contains($0.rawValue) } }

    var body: some View {
        let _ = (model.names, model.icons)
        List {
            if removedList.isEmpty {
                Text("Kaldırılan cihaz yok.").foregroundStyle(.secondary)
            } else {
                Section {
                    ForEach(removedList, id: \.self) { light in
                        HStack {
                            Label(light.title, systemImage: light.symbol)
                            Spacer()
                            Button("Geri ekle") { model.restore(light) }.buttonStyle(.bordered).tint(Color.lampOn)
                        }
                    }
                } footer: { Text("Cihazlar yalnızca uygulamadan ve widget'lardan kaldırılmıştı, ESP32 çalışmaya devam ediyor. Geri eklediğinde her şey eskisi gibi olur.") }
            }
        }
        .scrollContentBackground(.hidden)
        .background(Color.black.ignoresSafeArea())
        .navigationTitle("Kaldırılanlar")
        .navigationBarTitleDisplayMode(.inline)
    }
}
