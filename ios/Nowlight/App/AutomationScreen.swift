import SwiftUI

/// Kapi otomasyonu: kapinin ustundeki PIR dugumu odaya girisi algilar, secilen isiklari kendisi acar/kapatir/cevirir.
struct AutomationScreen: View {
    @State private var host = DoorConfig.host
    @State private var token = DoorConfig.token
    @State private var status: DoorStatus?
    @State private var message: String?
    @State private var busy = false
    @State private var poll: Task<Void, Never>?

    private let lampBits: [(String, String, Int)] = [("Masa", "lamp.desk.fill", 1), ("Yatak", "bed.double.fill", 2), ("LED", "light.strip.fill", 4)]

    var body: some View {
        Form {
            Section {
                TextField("Kapı düğümü adresi (örn. 192.168.1.51)", text: $host)
                    .textInputAutocapitalization(.never).autocorrectionDisabled().keyboardType(.URL)
                SecureField("Kapı düğümü token'ı", text: $token)
                    .textInputAutocapitalization(.never).autocorrectionDisabled()
                Button { Task { await connect() } } label: {
                    HStack { Text("Bağlan ve köprüyü tanıt"); if busy { Spacer(); ProgressView() } }
                }.disabled(busy || host.isEmpty || token.isEmpty)
                if let message { Text(message).font(.footnote).foregroundStyle(message.hasPrefix("✓") ? .green : .orange) }
            } header: { Text("Kapı düğümü") } footer: {
                Text("Adres ve token'ı kapı ESP32'sinin seri portunda `status` ve `token` yazarak görürsün. Bağlanınca köprünün adresi ve token'ı düğüme gönderilir; bir kere yapman yeterli.")
            }

            if let s = status {
                Section {
                    HStack {
                        Label("Hareket", systemImage: "figure.walk.motion")
                        Spacer()
                        Circle().fill(s.pir == 1 ? Color.lampOn : Color.gray.opacity(0.5)).frame(width: 12, height: 12)
                        Text(s.pir == 1 ? "algılandı" : (s.motions == 0 ? "henüz yok" : "\(s.motionAge) sn önce")).foregroundStyle(.secondary)
                    }
                    HStack {
                        Label("Saat", systemImage: "clock")
                        Spacer()
                        Text(s.timeSynced ? s.time : "alınamadı").foregroundStyle(.secondary)
                    }
                    Text(s.lastResult).font(.footnote).foregroundStyle(.secondary)
                } header: { Text("Canlı durum") }

                Section {
                    Toggle("Otomasyon açık", isOn: bind(\.enabled, "enabled") { $0 ? "1" : "0" }).tint(Color.lampOn)
                    Picker("Hareket olunca", selection: bind(\.mode, "mode") { String($0) }) {
                        Text("Işığı çevir (açıksa kapat, kapalıysa aç)").tag(0)
                        Text("Yalnızca aç").tag(1)
                        Text("Yalnızca kapat").tag(2)
                    }.pickerStyle(.inline).labelsHidden()
                } header: { Text("Davranış") }

                Section {
                    ForEach(lampBits, id: \.2) { name, icon, bit in
                        Toggle(isOn: lampBinding(bit)) { Label(name, systemImage: icon) }.tint(Color.lampOn)
                    }
                } header: { Text("Hangi ışıklar") } footer: { Text("En az bir ışık seçili kalmalı.") }

                Section {
                    Stepper(value: cooldownBinding(), in: 0...60) {
                        HStack { Text("Bekleme süresi"); Spacer(); Text(s.cooldown == 0 ? "yok" : "\(s.cooldown / 60) dk").foregroundStyle(.secondary) }
                    }
                } header: { Text("Bekleme") } footer: {
                    Text("Bir tetiklemeden sonra bu süre boyunca hareketler yok sayılır. 0 = her harekette çalışır (odaya ikinci biri girince önceki durum bozulmasın istiyorsan süreyi uzat).")
                }

                Section {
                    Toggle("Yalnızca saat aralığında", isOn: bind(\.winOn, "winOn") { $0 ? "1" : "0" }).tint(Color.lampOn)
                    if s.winOn {
                        DatePicker("Başlangıç", selection: timeBinding(\.winStart, "winStart"), displayedComponents: .hourAndMinute)
                        DatePicker("Bitiş", selection: timeBinding(\.winEnd, "winEnd"), displayedComponents: .hourAndMinute)
                    }
                } header: { Text("Saat aralığı") } footer: {
                    Text(s.winOn ? "Gece yarısını geçen aralıklar olur (örn. 18:00 – 07:00). Şu an aralığın \(s.inWindow ? "içinde" : "dışında")sin." : "Kapalıyken gün boyu çalışır.")
                }

                Section {
                    Button { Task { await runTest() } } label: { Label("Şimdi dene (ışıkları hemen uygula)", systemImage: "play.fill") }.disabled(busy)
                } footer: { Text("Seçili mod ve ışıklarla köprüye bir komut gönderir; bekleme süresi ve saat aralığına bakmaz.") }
            }
        }
        .navigationTitle("Otomasyon")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { startPolling() }
        .onDisappear { poll?.cancel() }
    }

    // MARK: ayar baglamalari: degisince hemen cihaza yazar, cihazin cevabini gosterir
    private func bind<T>(_ kp: KeyPath<DoorStatus, T>, _ key: String, _ enc: @escaping (T) -> String) -> Binding<T> {
        Binding(get: { status![keyPath: kp] }, set: { v in Task { await push([key: enc(v)]) } })
    }
    private func lampBinding(_ bit: Int) -> Binding<Bool> {
        Binding(get: { (status?.lamps ?? 0) & bit != 0 }, set: { on in
            var l = status?.lamps ?? 1
            l = on ? (l | bit) : (l & ~bit)
            if l == 0 { return }
            Task { await push(["lamps": String(l)]) }
        })
    }
    private func cooldownBinding() -> Binding<Int> {
        Binding(get: { (status?.cooldown ?? 0) / 60 }, set: { m in Task { await push(["cooldown": String(m * 60)]) } })
    }
    private func timeBinding(_ kp: KeyPath<DoorStatus, Int>, _ key: String) -> Binding<Date> {
        Binding(get: {
            let m = status?[keyPath: kp] ?? 0
            return Calendar.current.date(bySettingHour: m / 60, minute: m % 60, second: 0, of: Date()) ?? Date()
        }, set: { d in
            let c = Calendar.current.dateComponents([.hour, .minute], from: d)
            Task { await push([key: String((c.hour ?? 0) * 60 + (c.minute ?? 0))]) }
        })
    }

    // MARK: agi
    private func client() -> DoorClient { DoorClient(host: DoorConfig.host, token: DoorConfig.token) }

    private func push(_ f: [String: String]) async {
        do { status = try await client().configure(f); message = nil }
        catch { message = (error as? LocalizedError)?.errorDescription ?? "Kaydedilemedi."; await reload() }
    }

    private func reload() async {
        guard DoorConfig.isConfigured else { return }
        if let s = try? await client().status() { status = s }
    }

    private func startPolling() {
        poll?.cancel()
        poll = Task {
            while !Task.isCancelled {
                await reload()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    private func connect() async {
        busy = true; defer { busy = false }
        DoorConfig.host = host; DoorConfig.token = token
        let c = client()
        do {
            guard try await c.ping() else { message = "Bu adreste kapı düğümü yok."; return }
            guard BridgeConfig.isConfigured else { message = "Önce köprü ayarlarını (adres ve token) gir."; return }
            status = try await c.configure(["bridgeHost": BridgeConfig.host, "bridgeToken": BridgeConfig.token])
            message = "✓ Bağlandı, köprü bilgisi düğüme gönderildi."
            startPolling()
        } catch { message = (error as? LocalizedError)?.errorDescription ?? "Bağlantı kurulamadı." }
    }

    private func runTest() async {
        busy = true; defer { busy = false }
        do { status = try await client().test(); message = nil }
        catch { message = (error as? LocalizedError)?.errorDescription ?? "Deneme başarısız." }
    }
}
