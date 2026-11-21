import BackgroundTasks
import Foundation
import UserNotifications

/// Bildirim turleri: hangilerinin gelecegi uygulamadan secilir (Otomasyon > Bildirimler).
enum NotifyKind: String, CaseIterable, Identifiable {
    case program, timer, door, offline, update
    var id: String { rawValue }
    var title: String {
        switch self { case .program: "Program çalıştı"; case .timer: "Zamanlayıcı bitti"; case .door: "Kapı otomasyonu çalıştı"; case .offline: "Cihaz çevrimdışı"; case .update: "Yazılım güncellemesi" }
    }
    var detail: String {
        switch self {
        case .program: "Zamanlı bir program ışıkları ya da servoyu değiştirince."
        case .timer: "Uyku zamanlayıcısı bitip cihazları kapatınca/açınca."
        case .door: "Kapı sensörü bir kuralı çalıştırınca (ışıklar açılınca, çıkışta kapanınca)."
        case .offline: "Bir ışık düğümüne ulaşılamayınca."
        case .update: "Köprü ya da kapı düğümü yazılımı eskiyse."
        }
    }
    var symbol: String {
        switch self { case .program: "calendar.badge.clock"; case .timer: "timer"; case .door: "figure.walk.motion"; case .offline: "wifi.exclamationmark"; case .update: "arrow.down.circle" }
    }
}

enum NotifyPrefs {
    private static var d: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }
    /// Ana anahtar: kapaliyken hicbir bildirim gonderilmez ve arka plan denetimi calismaz.
    static var master: Bool {
        get { d.bool(forKey: "notify.master") }
        set { d.set(newValue, forKey: "notify.master") }
    }
    static func isOn(_ k: NotifyKind) -> Bool { master && (d.object(forKey: "notify.\(k.rawValue)") as? Bool ?? true) }
    static func rawOn(_ k: NotifyKind) -> Bool { d.object(forKey: "notify.\(k.rawValue)") as? Bool ?? true }
    static func set(_ k: NotifyKind, _ on: Bool) { d.set(on, forKey: "notify.\(k.rawValue)") }
    static var lastSeen: Int { get { d.integer(forKey: "notify.lastSeen") } set { d.set(newValue, forKey: "notify.lastSeen") } }
    static var knownDown: Set<String> {
        get { Set(d.stringArray(forKey: "notify.down") ?? []) }
        set { d.set(Array(newValue), forKey: "notify.down") }
    }
    static var updateSig: String { get { d.string(forKey: "notify.updSig") ?? "" } set { d.set(newValue, forKey: "notify.updSig") } }
}

/// Yerel bildirimler. Sunucu (push) yoktur: uygulama acikken ~20 sn'de bir, kapaliyken iOS'un izin verdigi kadar (genelde 15-60 dk)
/// kopru ve kapi dugumunun gunlugunu okur, yeni olay varsa bildirir. Yalnizca ev agindayken (ya da VPN ile) calisir.
@MainActor
enum NotificationService {
    static let taskID = "online.matestudios.nowlight.refresh"

    static func requestAuthorization() async -> Bool {
        (try? await UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge])) ?? false
    }

    static func send(_ kind: NotifyKind, title: String, body: String, id: String = UUID().uuidString) {
        guard NotifyPrefs.isOn(kind) else { return }
        let c = UNMutableNotificationContent()
        c.title = title; c.body = body; c.sound = .default
        UNUserNotificationCenter.current().add(UNNotificationRequest(identifier: id, content: c, trigger: nil))
    }

    /// Bir kez denetle: yeni olaylar, cevrimdisi dugumler, yazilim durumu.
    static func checkOnce() async {
        guard NotifyPrefs.master, BridgeConfig.isConfigured, let bridge = try? BridgeClient.fromConfig() else { return }
        var items: [HistoryItem] = []
        if let ev = try? await bridge.log() { items += ev.map(HistoryItem.from(bridge:)) }
        if DoorConfig.isConfigured, let ev = try? await DoorClient(host: DoorConfig.host, token: DoorConfig.token).log() { items += ev.map(HistoryItem.from(door:)) }
        let newest = items.compactMap { $0.date }.map { Int($0.timeIntervalSince1970) }.max() ?? 0
        let seen = NotifyPrefs.lastSeen
        if seen == 0 { NotifyPrefs.lastSeen = newest }                       // ilk calisma: eski olaylari bildirme
        else {
            for it in items.sorted(by: { ($0.date ?? .distantPast) < ($1.date ?? .distantPast) }) {
                guard let d = it.date, Int(d.timeIntervalSince1970) > seen else { continue }
                switch it.kind {
                case .program: send(.program, title: "Program çalıştı", body: it.text, id: it.id)
                case .timer: if !it.text.contains("kuruldu") { send(.timer, title: "Zamanlayıcı", body: it.text, id: it.id) }
                case .door: send(.door, title: "Kapı otomasyonu", body: it.text, id: it.id)
                case .command: break
                }
            }
            if newest > seen { NotifyPrefs.lastSeen = newest }
        }
        // Cevrimdisi dugumler: yeni kopanlar bildirilir
        if let s = try? await bridge.state() {
            let down = Set(LightID.devices.filter { !s.linked($0) }.map(\.rawValue))
            let fresh = down.subtracting(NotifyPrefs.knownDown)
            if !fresh.isEmpty { send(.offline, title: "Cihaz çevrimdışı", body: fresh.compactMap { LightID(rawValue: $0)?.title }.sorted().joined(separator: ", ") + " ulaşılamıyor") }
            NotifyPrefs.knownDown = down
        }
        // Yazilim: durum degisince bir kez
        let issues = await FirmwareCheck.run()
        let sig = issues.map(\.id).sorted().joined(separator: ",")
        if sig != NotifyPrefs.updateSig {
            NotifyPrefs.updateSig = sig
            if !issues.isEmpty { send(.update, title: "Yazılım güncellemesi", body: issues.map(\.title).joined(separator: ", ")) }
        }
    }

    // MARK: arka plan

    static func register() {
        BGTaskScheduler.shared.register(forTaskWithIdentifier: taskID, using: nil) { task in
            guard let t = task as? BGAppRefreshTask else { task.setTaskCompleted(success: false); return }
            scheduleRefresh()
            let work = Task { await checkOnce(); t.setTaskCompleted(success: true) }
            t.expirationHandler = { work.cancel(); t.setTaskCompleted(success: false) }
        }
    }

    static func scheduleRefresh() {
        guard NotifyPrefs.master else { return }
        let r = BGAppRefreshTaskRequest(identifier: taskID)
        r.earliestBeginDate = Date(timeIntervalSinceNow: 15 * 60)
        try? BGTaskScheduler.shared.submit(r)
    }
}
