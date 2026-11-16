import SwiftUI

/// Kapi otomasyonu: kapinin ustundeki PIR dugumu hareketi algilayinca, saati uyan ilk kural calisir.
struct AutomationScreen: View {
    @State private var store = AutomationStore()
    @State private var host = DoorConfig.host
    @State private var token = DoorConfig.token
    @State private var message: String?
    @State private var busy = false
    @State private var editing: AutomationRule?
    @State private var isNew = false

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
                Text("Adres ve token'ı kapı ESP32'sinin seri portunda `status` ve `token` yazarak görürsün. Bir kere bağlamak yeterli.")
            }

            if let s = store.status {
                Section {
                    Toggle("Otomasyon açık", isOn: Binding(get: { s.enabled }, set: { v in Task { await store.setMaster(v) } })).tint(Color.lampOn)
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
                } header: { Text("Durum") }
            }

            if DoorConfig.isConfigured || !store.rules.isEmpty {
                Section {
                    if store.rules.isEmpty {
                        Text("Henüz kural yok. Bir kural: \"şu saatler arasında hareket olursa şu ışıkları aç / kapat / çevir\".").font(.subheadline).foregroundStyle(.secondary)
                        Button { store.addDayNightTemplate() } label: { Label("Hazır: akşam aç, gündüz kapat", systemImage: "wand.and.stars") }
                    }
                    ForEach(Array(store.rules.enumerated()), id: \.element.id) { i, r in
                        Button { isNew = false; editing = r } label: { RuleRow(rule: r, index: i, active: store.status?.activeRule == i) }
                            .buttonStyle(.plain)
                    }
                    .onDelete { store.delete(at: $0) }
                    .onMove { store.move(from: $0, to: $1) }
                    Button { isNew = true; editing = AutomationRule() } label: { Label("Kural ekle", systemImage: "plus.circle.fill") }
                        .disabled(store.rules.count >= 8)
                } header: {
                    HStack { Text("Kurallar"); Spacer(); SyncBadge(sync: store.sync) }
                } footer: {
                    Text("Harekette saati uyan ilk kural çalışır; kuralların sırası önemli (sürükleyerek değiştir). En fazla 8 kural. Düğüme her değişiklikte gönderilir.")
                }
            }
        }
        .navigationTitle("Otomasyon")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { EditButton() }
        .onAppear { store.start() }
        .onDisappear { store.stop() }
        .sheet(item: $editing) { r in
            RuleEditor(rule: r, isNew: isNew, testEnabled: store.sync == .synced) { saved in store.upsert(saved) } onTest: { saved in
                Task {
                    if let i = store.rules.firstIndex(where: { $0.id == saved.id }) {
                        do { store.status = try await store.client.test(rule: i) } catch { message = (error as? LocalizedError)?.errorDescription }
                    }
                }
            }
        }
    }

    private func connect() async {
        busy = true; defer { busy = false }
        DoorConfig.host = host; DoorConfig.token = token
        let c = store.client
        do {
            guard try await c.ping() else { message = "Bu adreste kapı düğümü yok."; return }
            guard BridgeConfig.isConfigured else { message = "Önce köprü ayarlarını (adres ve token) gir."; return }
            store.status = try await c.configure(["bridgeHost": BridgeConfig.host, "bridgeToken": BridgeConfig.token])
            message = "✓ Bağlandı, köprü bilgisi düğüme gönderildi."
            store.start()
        } catch { message = (error as? LocalizedError)?.errorDescription ?? "Bağlantı kurulamadı." }
    }
}

private struct SyncBadge: View {
    let sync: AutomationStore.Sync
    var body: some View {
        let (text, color): (String, Color) = switch sync {
        case .synced: ("düğümde", .green)
        case .pending: ("gönderiliyor…", .orange)
        case .oldFirmware: ("düğüm yazılımı güncel değil", .orange)
        case .offline: ("düğüme ulaşılamıyor", .orange)
        case .unknown: ("", .secondary)
        }
        Text(text).font(.caption).textCase(nil).foregroundStyle(color)
    }
}

private struct RuleRow: View {
    let rule: AutomationRule
    let index: Int
    let active: Bool
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: rule.action == .on ? "lightbulb.fill" : (rule.action == .off ? "lightbulb.slash.fill" : "arrow.triangle.2.circlepath"))
                .foregroundStyle(.black).frame(width: 36, height: 36)
                .background(rule.enabled ? Color.lampOn : Color.gray.opacity(0.5), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            VStack(alignment: .leading, spacing: 2) {
                HStack(spacing: 6) {
                    Text(rule.name).font(.body.weight(.semibold))
                    if active { Text("şu an geçerli").font(.caption2.weight(.semibold)).padding(.horizontal, 6).padding(.vertical, 1).background(Color.green.opacity(0.25), in: Capsule()) }
                }
                Text("\(rule.action.title) · \(rule.lampText)").font(.subheadline).foregroundStyle(.secondary)
                Text("\(rule.windowText)\(rule.cooldownMin > 0 ? " · bekleme \(rule.cooldownMin) dk" : "")").font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
        }
        .opacity(rule.enabled ? 1 : 0.55)
        .contentShape(Rectangle())
    }
}

private struct RuleEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State var rule: AutomationRule
    let isNew: Bool
    let testEnabled: Bool
    let onSave: (AutomationRule) -> Void
    let onTest: (AutomationRule) -> Void

    private var lampBits: [(String, String, Int)] {
        [(LightID.masa, 1), (LightID.yatak, 2), (LightID.led, 4)].filter { LightID.devices.contains($0.0) }.map { ($0.0.title, $0.0.symbol, $0.1) }
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Kural adı", text: $rule.name)
                    Toggle("Kural açık", isOn: $rule.enabled).tint(Color.lampOn)
                }
                Section {
                    Picker("Hareket olunca", selection: $rule.action) {
                        ForEach(AutomationRule.Action.allCases, id: \.self) { Text($0.title).tag($0) }
                    }.pickerStyle(.segmented)
                } footer: { Text("Çevir: açıksa kapatır, kapalıysa açar. Aç ve Kapat: durumdan bağımsız.") }
                Section("Hangi ışıklar") {
                    ForEach(lampBits, id: \.2) { name, icon, bit in
                        Toggle(isOn: Binding(get: { rule.lamps & bit != 0 }, set: { on in
                            let l = on ? rule.lamps | bit : rule.lamps & ~bit
                            if l & AutomationRule.visibleMask != 0 { rule.lamps = l }
                        })) { Label(name, systemImage: icon) }.tint(Color.lampOn)
                    }
                }
                Section {
                    Toggle("Gün boyu", isOn: Binding(get: { rule.start == rule.end }, set: { on in
                        if on { rule.start = 0; rule.end = 0 } else { rule.start = 18 * 60; rule.end = 6 * 60 }
                    })).tint(Color.lampOn)
                    if rule.start != rule.end {
                        DatePicker("Başlangıç", selection: time(\.start), displayedComponents: .hourAndMinute)
                        DatePicker("Bitiş", selection: time(\.end), displayedComponents: .hourAndMinute)
                    }
                } header: { Text("Saat aralığı") } footer: {
                    Text("Bitiş saati dahil değil: 06:00 – 18:00 aralığı 17:59'da biter. Gece yarısını geçen aralık olur (18:00 – 06:00).")
                }
                Section {
                    Stepper(value: $rule.cooldownMin, in: 0...60) {
                        HStack { Text("Bekleme süresi"); Spacer(); Text(rule.cooldownMin == 0 ? "yok" : "\(rule.cooldownMin) dk").foregroundStyle(.secondary) }
                    }
                } header: { Text("Bekleme") } footer: { Text("Bu kural çalıştıktan sonra bu süre boyunca hareketler yok sayılır. 0: her harekette çalışır.") }
                if testEnabled && !isNew {
                    Section {
                        Button { onSave(rule); onTest(rule) } label: { Label("Şimdi dene (saatine bakmaz)", systemImage: "play.fill") }
                    } footer: { Text("Kuralı kaydeder ve ışıklara hemen uygular.") }
                }
            }
            .navigationTitle(isNew ? "Yeni kural" : "Kural")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Vazgeç") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) { Button("Kaydet") { onSave(rule); dismiss() } }
            }
        }
    }

    private func time(_ kp: WritableKeyPath<AutomationRule, Int>) -> Binding<Date> {
        Binding(get: {
            let m = rule[keyPath: kp]
            return Calendar.current.date(bySettingHour: m / 60, minute: m % 60, second: 0, of: Date()) ?? Date()
        }, set: { d in
            let c = Calendar.current.dateComponents([.hour, .minute], from: d)
            rule[keyPath: kp] = (c.hour ?? 0) * 60 + (c.minute ?? 0)
        })
    }
}
