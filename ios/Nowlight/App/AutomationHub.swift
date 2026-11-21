import SwiftUI

extension ScheduleRule {
    /// Bu programin bir sonraki calisma zamani (en fazla 8 gun ileri). Kapali programda nil.
    func nextRun(after now: Date = Date()) -> Date? {
        guard enabled, effectiveDevices != 0 else { return nil }
        let cal = Calendar.current
        for add in 0...7 {
            guard let day = cal.date(byAdding: .day, value: add, to: now),
                  let at = cal.date(bySettingHour: minute / 60, minute: minute % 60, second: 0, of: day), at > now else { continue }
            let wday = (cal.component(.weekday, from: at) + 5) % 7                   // Pazartesi = 0
            if (days >> wday) & 1 == 1 { return at }
        }
        return nil
    }
}

/// Otomasyon sekmesi: sahneler, programlar, zamanlayicilar, kapi otomasyonu, gecmis ve bildirimler tek yerde.
struct AutomationHubScreen: View {
    @Environment(AppModel.self) private var model
    @State private var nextText = ""

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    link(ScenesScreen(), "Sahneler", "sparkles", model.scenes.isEmpty ? "Birkaç cihazı tek dokunuşla ayarla" : "\(model.scenes.count) sahne")
                    link(ScheduleScreen(), "Programlar", "calendar.badge.clock", nextText.isEmpty ? "Saat ve güne bağlı otomasyon" : nextText)
                    link(TimersScreen(), "Zamanlayıcılar", "timer", "\"30 dk sonra kapat\"")
                    link(AutomationScreen(), "Kapı otomasyonu", "figure.walk.motion", "Hareket ve gir-çık kuralları")
                } header: { Text("Otomasyonlar") }

                Section {
                    link(HistoryScreen(), "Geçmiş", "clock.arrow.circlepath", "Neyin ne zaman çalıştığı")
                    link(NotificationSettingsScreen(), "Bildirimler", "bell.badge", NotifyPrefs.master ? "Açık" : "Kapalı")
                } header: { Text("Takip") }
            }
            .navigationTitle("Otomasyon")
            .task { nextText = nextProgramText() }
        }
    }

    private func link<D: View>(_ dest: D, _ title: String, _ symbol: String, _ sub: String) -> some View {
        NavigationLink { dest } label: {
            HStack(spacing: 12) {
                Image(systemName: symbol).foregroundStyle(.black).frame(width: 34, height: 34)
                    .background(Color.lampOn, in: RoundedRectangle(cornerRadius: 9, style: .continuous))
                VStack(alignment: .leading, spacing: 1) {
                    Text(title)
                    Text(sub).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                }
            }
        }
    }

    /// "Sonraki: bugün 19:00 · Akşam aç"
    private func nextProgramText() -> String {
        let rules = ScheduleStore().rules
        guard let best = rules.compactMap({ r in r.nextRun().map { (r, $0) } }).min(by: { $0.1 < $1.1 }) else {
            return rules.isEmpty ? "" : "Etkin program yok"
        }
        let cal = Calendar.current
        let day = cal.isDateInToday(best.1) ? "bugün" : (cal.isDateInTomorrow(best.1) ? "yarın" : best.1.formatted(.dateTime.weekday(.wide)))
        return "Sonraki: \(day) \(best.0.timeText) · \(best.0.name)"
    }
}

// MARK: Zamanlayicilar

struct TimersScreen: View {
    @Environment(AppModel.self) private var model
    @State private var timers: [BridgeClient.TimerInfo] = []
    @State private var message: String?
    @State private var showNew = false
    @State private var oldBridge = false

    var body: some View {
        Form {
            Section {
                if oldBridge {
                    Label("Köprü yazılımı zamanlayıcıyı bilmiyor, USB ile güncelle.", systemImage: "exclamationmark.triangle.fill").font(.footnote).foregroundStyle(.orange)
                }
                ForEach(timers) { t in
                    TimelineView(.periodic(from: .now, by: 1)) { ctx in
                        HStack {
                            Image(systemName: "timer").foregroundStyle(Color.lampOn)
                            VStack(alignment: .leading, spacing: 2) {
                                Text(remaining(t, at: ctx.date)).font(.system(.title3, design: .rounded).weight(.semibold)).monospacedDigit()
                                Text("\(t.action == 1 ? "Aç" : "Kapat") · \(deviceNames(t.devs))").font(.caption).foregroundStyle(.secondary)
                            }
                            Spacer()
                            Button("İptal", role: .destructive) { Task { await cancel(t) } }.buttonStyle(.bordered)
                        }
                    }
                }
                if timers.isEmpty && !oldBridge { Text("Çalışan zamanlayıcı yok.").font(.subheadline).foregroundStyle(.secondary) }
            } header: { Text("Çalışanlar") }

            Section {
                ForEach([15, 30, 60, 120], id: \.self) { m in
                    Button { Task { await add(minutes: m, devs: lightMask, action: 2) } } label: { Label("\(m < 60 ? "\(m) dk" : "\(m / 60) saat") sonra tüm ışıkları kapat", systemImage: "moon.zzz.fill") }
                }
                Button { showNew = true } label: { Label("Özel zamanlayıcı…", systemImage: "slider.horizontal.3") }
                if let message { Text(message).font(.footnote).foregroundStyle(message.hasPrefix("✓") ? .green : .orange) }
            } header: { Text("Yeni") } footer: {
                Text("Zamanlayıcı köprüde çalışır, telefon kapalıyken de işler. Köprü yeniden başlarsa (elektrik kesintisi) zamanlayıcılar silinir. En fazla 4 zamanlayıcı.")
            }
        }
        .navigationTitle("Zamanlayıcılar")
        .navigationBarTitleDisplayMode(.inline)
        .task { while !Task.isCancelled { await load(); try? await Task.sleep(nanoseconds: 5_000_000_000) } }
        .sheet(isPresented: $showNew) { NewTimerSheet { m, d, a in Task { await add(minutes: m, devs: d, action: a) } } }
    }

    private var lightMask: Int { LightID.lights.reduce(0) { $0 | $1.bit } }

    private func deviceNames(_ mask: Int) -> String {
        let l = LightID.controllable.filter { mask & $0.bit != 0 }
        return l.count == LightID.controllable.count && l.count > 1 ? "tüm cihazlar" : l.map(\.title).joined(separator: ", ")
    }
    private func remaining(_ t: BridgeClient.TimerInfo, at date: Date) -> String {
        let left = max(0, t.left - Int(date.timeIntervalSince(loadedAt)))
        return left >= 3600 ? String(format: "%d:%02d:%02d", left / 3600, left % 3600 / 60, left % 60) : String(format: "%02d:%02d", left / 60, left % 60)
    }
    @State private var loadedAt = Date()

    private func load() async {
        guard let c = try? BridgeClient.fromConfig() else { return }
        do { timers = try await c.timers(); loadedAt = Date(); oldBridge = false }
        catch BridgeError.badResponse { oldBridge = true } catch { }
    }
    private func add(minutes: Int, devs: Int, action: Int) async {
        guard let c = try? BridgeClient.fromConfig() else { return }
        do { timers = try await c.addTimer(minutes: minutes, devs: devs, action: action); loadedAt = Date(); message = "✓ Zamanlayıcı kuruldu" }
        catch { message = (error as? LocalizedError)?.errorDescription ?? "Kurulamadı (en fazla 4 zamanlayıcı)" }
    }
    private func cancel(_ t: BridgeClient.TimerInfo) async {
        try? await BridgeClient.fromConfig().cancelTimer(id: t.id); await load()
    }
}

private struct NewTimerSheet: View {
    @Environment(\.dismiss) private var dismiss
    var onSave: (Int, Int, Int) -> Void
    @State private var minutes = 30
    @State private var action = 2
    @State private var mask = LightID.lights.reduce(0) { $0 | $1.bit }

    var body: some View {
        NavigationStack {
            Form {
                Section("Ne zaman") {
                    Picker("Süre", selection: $minutes) { ForEach([5, 10, 15, 30, 45, 60, 90, 120, 180, 240, 360], id: \.self) { Text($0 < 60 ? "\($0) dk" : "\($0 / 60) saat" + ($0 % 60 == 0 ? "" : " \($0 % 60) dk")).tag($0) } }
                }
                Section("Ne yapsın") {
                    Picker("Eylem", selection: $action) { Text("Kapat").tag(2); Text("Aç").tag(1) }.pickerStyle(.segmented)
                }
                Section("Hangi cihazlar") {
                    ForEach(LightID.controllable, id: \.self) { d in
                        Toggle(isOn: Binding(get: { mask & d.bit != 0 }, set: { on in mask = on ? mask | d.bit : mask & ~d.bit })) { Label(d.title, systemImage: d.symbol) }.tint(Color.lampOn)
                    }
                }
            }
            .navigationTitle("Zamanlayıcı")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Vazgeç") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) { Button("Kur") { onSave(minutes, mask, action); dismiss() }.disabled(mask == 0) }
            }
        }
        .preferredColorScheme(.dark)
    }
}

// MARK: Gecmis

struct HistoryScreen: View {
    @State private var items: [HistoryItem] = []
    @State private var hidden: Set<HistoryItem.Kind> = [.command]
    @State private var loading = true
    @State private var note: String?

    private var shown: [HistoryItem] { items.filter { !hidden.contains($0.kind) } }

    var body: some View {
        List {
            if let note { Section { Label(note, systemImage: "exclamationmark.triangle.fill").font(.footnote).foregroundStyle(.orange) } }
            if shown.isEmpty && !loading {
                Section { Text("Henüz kayıt yok. Programlar, zamanlayıcılar ve kapı otomasyonu çalışınca burada görünür. Kayıtlar köprü ya da kapı düğümü yeniden başlayınca silinir.").font(.subheadline).foregroundStyle(.secondary) }
            }
            ForEach(shown) { it in
                HStack(spacing: 12) {
                    Image(systemName: it.kind.symbol).foregroundStyle(Color.lampOn).frame(width: 26)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(it.text)
                        Text("\(it.kind.title) · \(it.date.map { $0.formatted(date: .abbreviated, time: .standard) } ?? "saat bilinmiyor")").font(.caption).foregroundStyle(.secondary)
                    }
                }
            }
        }
        .navigationTitle("Geçmiş")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar {
            ToolbarItem(placement: .topBarTrailing) {
                Menu {
                    ForEach(HistoryItem.Kind.allCases, id: \.self) { k in
                        Button { if hidden.contains(k) { hidden.remove(k) } else { hidden.insert(k) } } label: { Label(k.title, systemImage: hidden.contains(k) ? "circle" : "checkmark.circle.fill") }
                    }
                } label: { Image(systemName: "line.3.horizontal.decrease.circle") }
            }
        }
        .refreshable { await load() }
        .task { await load() }
    }

    private func load() async {
        defer { loading = false }
        var all: [HistoryItem] = []; var msgs: [String] = []
        if BridgeConfig.isConfigured, let c = try? BridgeClient.fromConfig() {
            do { all += try await c.log().map(HistoryItem.from(bridge:)) } catch BridgeError.badResponse { msgs.append("Köprü yazılımı geçmişi bilmiyor (USB ile güncelle).") } catch { msgs.append("Köprüye ulaşılamadı.") }
        }
        if DoorConfig.isConfigured {
            do { all += try await DoorClient(host: DoorConfig.host, token: DoorConfig.token).log().map(HistoryItem.from(door:)) } catch BridgeError.badResponse { msgs.append("Kapı düğümü yazılımı geçmişi bilmiyor (USB ile güncelle).") } catch { }
        }
        items = all.sorted { ($0.date ?? .distantPast) > ($1.date ?? .distantPast) }
        note = msgs.first
    }
}

// MARK: Bildirimler

struct NotificationSettingsScreen: View {
    @State private var master = NotifyPrefs.master
    @State private var kinds: [NotifyKind: Bool] = Dictionary(uniqueKeysWithValues: NotifyKind.allCases.map { ($0, NotifyPrefs.rawOn($0)) })
    @State private var denied = false

    var body: some View {
        Form {
            Section {
                Toggle("Bildirimleri aç", isOn: Binding(get: { master }, set: { on in
                    Task {
                        if on { let ok = await NotificationService.requestAuthorization(); denied = !ok; master = ok } else { master = false; denied = false }
                        NotifyPrefs.master = master
                        if master { NotificationService.scheduleRefresh() }
                    }
                })).tint(Color.lampOn)
                if denied { Text("Bildirim izni verilmedi. Ayarlar > Nowlight > Bildirimler'den izin ver.").font(.footnote).foregroundStyle(.orange) }
            } footer: {
                Text("Sunucu olmadığı için bildirimler telefonun köprüyü kontrol etmesiyle gelir: uygulama açıkken yaklaşık 20 saniyede bir, kapalıyken iOS'un izin verdiği kadar (genelde 15 dakika - birkaç saat). Evdeki WiFi'ye (ya da VPN'e) bağlıyken çalışır; bu yüzden gecikebilir.")
            }
            Section {
                ForEach(NotifyKind.allCases) { k in
                    Toggle(isOn: Binding(get: { kinds[k] ?? true }, set: { kinds[k] = $0; NotifyPrefs.set(k, $0) })) {
                        VStack(alignment: .leading, spacing: 2) {
                            Label(k.title, systemImage: k.symbol)
                            Text(k.detail).font(.caption).foregroundStyle(.secondary)
                        }
                    }.tint(Color.lampOn).disabled(!master)
                }
            } header: { Text("Hangileri gelsin?") }
        }
        .navigationTitle("Bildirimler")
        .navigationBarTitleDisplayMode(.inline)
    }
}
