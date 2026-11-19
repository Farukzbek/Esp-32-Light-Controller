import SwiftUI

/// Programlar: saate ve gune bagli otomasyon. Kopruda calisir, telefon kapaliyken de tetiklenir.
struct ScheduleScreen: View {
    @Environment(AppModel.self) private var model
    @State private var store = ScheduleStore()
    @State private var editing: ScheduleRule?
    @State private var isNew = false

    var body: some View {
        Form {
            Section {
                HStack {
                    Label("Köprünün saati", systemImage: "clock")
                    Spacer()
                    Text(store.info?.synced == true ? (store.info?.time ?? "—") : "alınamadı").foregroundStyle(.secondary)
                }
                SyncRow(sync: store.sync)
            } header: { Text("Durum") } footer: {
                Text("Köprü saati internetten alır. Saat alınamıyorsa (köprü internete bağlı değilse) programlar çalışmaz. Elektrik kesintisinde kaçırılan programlar atlanır.")
            }

            Section {
                if store.rules.isEmpty {
                    Text("Henüz program yok. Bir program: \"her gün şu saatte şu cihazları aç / kapat\", servolar için \"şu açıya git\".").font(.subheadline).foregroundStyle(.secondary)
                    Button { store.addTemplate("perde") } label: { Label("Hazır: sabah perdeyi aç (Pazar hariç)", systemImage: "wand.and.stars") }
                    Button { store.addTemplate("gece") } label: { Label("Hazır: gece hepsini kapat (23:00)", systemImage: "moon.stars") }
                }
                ForEach(store.rules) { r in
                    Button { isNew = false; editing = r } label: { ScheduleRow(rule: r) }.buttonStyle(.plain)
                }
                .onDelete { store.delete(at: $0) }
                Button { isNew = true; editing = ScheduleRule() } label: { Label("Program ekle", systemImage: "plus.circle.fill") }
                    .disabled(store.rules.count >= 16)
            } header: { Text("Programlar") } footer: {
                Text("En fazla 16 program. Her değişiklikte köprüye gönderilir. Kapı hareket otomasyonundan bağımsızdır, ikisi birlikte çalışabilir.")
            }
        }
        .navigationTitle("Programlar")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { EditButton() }
        .task { await store.refresh() }
        .sheet(item: $editing) { r in
            ScheduleEditor(rule: r, isNew: isNew) { store.upsert($0) }
        }
    }
}

private struct SyncRow: View {
    let sync: ScheduleStore.Sync
    var body: some View {
        let (text, color, icon): (String, Color, String) = switch sync {
        case .synced: ("Köprüde kayıtlı", .green, "checkmark.circle.fill")
        case .pending: ("Gönderiliyor…", .orange, "arrow.triangle.2.circlepath")
        case .oldBridge: ("Köprü yazılımı programları bilmiyor, güncelle", .orange, "exclamationmark.triangle.fill")
        case .offline: ("Köprüye ulaşılamadı: program telefonda bekliyor", .orange, "wifi.slash")
        case .unknown: ("Kontrol ediliyor…", .secondary, "clock")
        }
        Label(text, systemImage: icon).font(.subheadline).foregroundStyle(color)
    }
}

private struct ScheduleRow: View {
    let rule: ScheduleRule
    var body: some View {
        HStack(spacing: 12) {
            Text(rule.timeText).font(.system(size: 26, weight: .semibold, design: .rounded)).monospacedDigit()
                .foregroundStyle(rule.enabled ? Color.lampOn : .secondary).frame(width: 78, alignment: .leading)
            VStack(alignment: .leading, spacing: 2) {
                Text(rule.name).font(.body.weight(.semibold))
                Text("\(rule.actionText) · \(rule.deviceText)").font(.subheadline).foregroundStyle(.secondary)
                Text(AutomationRule.daySummary(rule.days)).font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
        }
        .opacity(rule.enabled ? 1 : 0.55)
        .contentShape(Rectangle())
    }
}

private struct ScheduleEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State var rule: ScheduleRule
    let isNew: Bool
    var onSave: (ScheduleRule) -> Void

    private var devices: [LightID] { LightID.controllable }
    private var hasServo: Bool { devices.contains { $0.role == .servo && rule.devices & $0.bit != 0 } }

    private var time: Binding<Date> {
        Binding(get: { Calendar.current.date(bySettingHour: rule.minute / 60, minute: rule.minute % 60, second: 0, of: Date()) ?? Date() },
                set: { let c = Calendar.current.dateComponents([.hour, .minute], from: $0); rule.minute = (c.hour ?? 0) * 60 + (c.minute ?? 0) })
    }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Ad", text: $rule.name)
                    Toggle("Program açık", isOn: $rule.enabled).tint(Color.lampOn)
                }
                Section {
                    DatePicker("Saat", selection: time, displayedComponents: .hourAndMinute)
                } header: { Text("Ne zaman") }
                Section {
                    DayPicker(days: $rule.days).padding(.vertical, 4)
                } header: { Text("Hangi günler") }
                Section {
                    Picker("Eylem", selection: $rule.action) {
                        ForEach(ScheduleRule.Action.allCases, id: \.self) { Text($0.title).tag($0) }
                    }.pickerStyle(.segmented)
                    if rule.action == .angle {
                        VStack(alignment: .leading, spacing: 6) {
                            HStack { Text("Açı"); Spacer(); Text("\(rule.angle)°").foregroundStyle(.secondary) }
                            Slider(value: Binding(get: { Double(rule.angle) }, set: { rule.angle = Int($0) }), in: 0...180, step: 5).tint(Color.lampOn)
                        }
                    }
                } header: { Text("Ne yapsın") } footer: {
                    Text(rule.action == .angle ? "\"Açıya git\" yalnızca servolar için; ışıklara uygulanmaz." : rule.action == .toggle ? "Çevir: açıksa kapatır, kapalıysa açar. Servolarda çevir anlamlı değildir." : "Servolarda Aç = açık açısına, Kapat = kapalı açısına gider (cihaz ayarlarından).")
                }
                Section("Hangi cihazlar") {
                    ForEach(devices, id: \.self) { d in
                        Toggle(isOn: Binding(get: { rule.devices & d.bit != 0 }, set: { on in rule.devices = on ? rule.devices | d.bit : rule.devices & ~d.bit })) {
                            Label(d.title, systemImage: d.symbol)
                        }.tint(Color.lampOn)
                    }
                }
            }
            .navigationTitle(isNew ? "Yeni program" : "Program")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Vazgeç") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Kaydet") { onSave(rule); dismiss() }.disabled(rule.effectiveDevices == 0 || (rule.action == .angle && !hasServo))
                }
            }
        }
        .preferredColorScheme(.dark)
    }
}
