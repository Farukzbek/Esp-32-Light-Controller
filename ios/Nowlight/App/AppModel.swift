import Foundation
import Observation
import WidgetKit
import AppIntents

/// Uygulamanin durumu: koprudan gelen isik durumlari, baglanti hatasi ve dokunulunca komut gonderme.
@MainActor
@Observable
final class AppModel {
    var state: LightsState?
    var error: String?
    var lastUpdate: Date?
    private var polling: Task<Void, Never>?
    private var deletedAt: [String: Date] = [:]       // yeni silinen isiklar: kopru yeniden baslayana kadar benimsenmez
    private var pendingUntil: [LightID: Date] = [:]   // komut gonderilen isigin eski cevapla geri donmemesi icin

    var connected: Bool { state != nil && error == nil }

    /// Kaldirilan cihazlar (BridgeConfig ile esit tutulur; gorunumler bunu izler)
    var removed: Set<String> = BridgeConfig.removed

    /// Kullanici adlari (gorunumler bunu izler)
    var names: [String: String] = BridgeConfig.names

    /// Kullanici simgeleri
    var icons: [String: String] = BridgeConfig.icons

    /// Cihaz rolleri (servo, PIR). Kopru bildirir; gorunumler bunu izler.
    var roles: [String: String] = BridgeConfig.roles

    func setIcon(_ light: LightID, _ symbol: String?) {
        if let symbol, symbol != light.defaultSymbol { icons[light.rawValue] = symbol } else { icons.removeValue(forKey: light.rawValue) }
        BridgeConfig.icons = icons
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
    }

    func rename(_ light: LightID, to name: String) {
        let n = String(name.trimmingCharacters(in: .whitespacesAndNewlines).prefix(20))
        if n.isEmpty { names.removeValue(forKey: light.rawValue) } else { names[light.rawValue] = n }
        BridgeConfig.names = names
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
    }

    /// Yeni bir isik ekler (kopruyle eslestikten sonra). Ad ve simge uygulamada saklanir.
    func addDevice(_ light: LightID, name: String, symbol: String, pin: Int? = nil, inv: Bool? = nil, role: NodeRole = .light, servoOff: Int = 0, servoOn: Int = 90) {
        if role != .light { roles[light.rawValue] = role.rawValue; BridgeConfig.roles = roles }
        var rs = BridgeConfig.roleSettings
        if role == .servo { rs[light.rawValue] = ServoSettings(pin: pin ?? 5, off: servoOff, on: servoOn).stored }
        if role == .pir { rs[light.rawValue] = PirSettings(pin: pin ?? 4).stored }
        BridgeConfig.roleSettings = rs
        if role == .light, let pin, let inv { var r = BridgeConfig.relay; r[light.rawValue] = "\(pin):\(inv ? 1 : 0)"; BridgeConfig.relay = r }
        var list = BridgeConfig.customDevices
        if !list.contains(light.rawValue) { list.append(light.rawValue) }
        BridgeConfig.customDevices = list
        removed.remove(light.rawValue); BridgeConfig.removed = removed
        rename(light, to: name)
        setIcon(light, symbol)
        AutomationStore.resyncStored()
    }

    /// Eklenen isigin role ayarini (pin, ters mantik) dugume gonderir ve telefonda saklar.
    func setRelay(_ light: LightID, pin: Int, inv: Bool) async throws {
        guard light.role == .light, !light.isBuiltin || light == .masa || light == .yatak else { return }
        try await BridgeClient.fromConfig().nodeConfig(id: light.rawValue, pin: pin, inv: inv)
        var r = BridgeConfig.relay; r[light.rawValue] = "\(pin):\(inv ? 1 : 0)"; BridgeConfig.relay = r
    }

    /// Servo ayari: dugume gonderilir ve telefonda saklanir.
    func setServo(_ light: LightID, _ s: ServoSettings) async throws {
        try await BridgeClient.fromConfig().servoConfig(id: light.rawValue, pin: s.pin, inv: s.inv, off: s.off, on: s.on, speed: s.speed, release: s.release, min: s.min, max: s.max)
        var rs = BridgeConfig.roleSettings; rs[light.rawValue] = s.stored; BridgeConfig.roleSettings = rs
    }
    func servoSettings(_ light: LightID) -> ServoSettings { ServoSettings(stored: BridgeConfig.roleSettings[light.rawValue]) ?? ServoSettings() }

    /// PIR ayari: dugume gonderilir ve telefonda saklanir.
    func setPir(_ light: LightID, _ s: PirSettings) async throws {
        try await BridgeClient.fromConfig().pirConfig(id: light.rawValue, pin: s.pin, hold: s.hold, confirm: s.confirm, cooldown: s.cooldown, inv: s.inv)
        var rs = BridgeConfig.roleSettings; rs[light.rawValue] = s.stored; BridgeConfig.roleSettings = rs
    }
    func pirSettings(_ light: LightID) -> PirSettings { PirSettings(stored: BridgeConfig.roleSettings[light.rawValue]) ?? PirSettings() }

    /// Servoyu bir acyi gonderir (kaydirici). Aciyi hemen gosterir; kopru onaylayinca gercek deger gelir.
    func servoGoto(_ light: LightID, angle: Int) async {
        if var s = state { s.servo[light.rawValue] = LightsState.ServoInfo(angle: angle, free: false); state = s }
        do { try await BridgeClient.fromConfig().servoGoto(id: light.rawValue, angle: angle); error = nil }
        catch { self.error = (error as? LocalizedError)?.errorDescription ?? "Komut gönderilemedi" }
    }

    /// Servoyu birakir: PWM kesilir, elle oynatilabilir.
    func servoFree(_ light: LightID) async {
        do { try await BridgeClient.fromConfig().servoFree(id: light.rawValue); error = nil
            try? await Task.sleep(nanoseconds: 600_000_000); await refresh()
        } catch { self.error = (error as? LocalizedError)?.errorDescription ?? "Komut gönderilemedi" }
    }

    /// Eklenen isigi KALICI siler: dugumun eslesmesi silinir (kopru kisaca yeniden baslar), uygulamadan ve otomasyonlardan kalkar.
    func deleteDevice(_ light: LightID) async throws {
        guard !light.isBuiltin else { return }
        // Kopru bu isigi zaten bilmiyorsa (400: ornegin daha once silindi, hayalet kayit) yerelden silmek yeterli; yalnizca ag hatasinda dur.
        do { try await BridgeClient.fromConfig().nodeForget(id: light.rawValue) }
        catch BridgeError.badResponse { }
        deletedAt[light.rawValue] = Date()           // kopru yeniden baslayana kadar eski durumda goruneceginden hemen geri eklenmesin
        BridgeConfig.customDevices = BridgeConfig.customDevices.filter { $0 != light.rawValue }
        removed.remove(light.rawValue); BridgeConfig.removed = removed
        names.removeValue(forKey: light.rawValue); BridgeConfig.names = names
        icons.removeValue(forKey: light.rawValue); BridgeConfig.icons = icons
        var r = BridgeConfig.relay; r.removeValue(forKey: light.rawValue); BridgeConfig.relay = r
        roles.removeValue(forKey: light.rawValue); BridgeConfig.roles = roles
        var rs = BridgeConfig.roleSettings; rs.removeValue(forKey: light.rawValue); BridgeConfig.roleSettings = rs
        AutomationStore.clearBit(light.bit)
        ScheduleStore.clearBit(light.bit)
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
        AutomationStore.resyncStored()
    }

    func remove(_ light: LightID) {
        removed.insert(light.rawValue)
        BridgeConfig.removed = removed
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
        AutomationStore.resyncStored()
    }

    func restore(_ light: LightID) {
        removed.remove(light.rawValue)
        BridgeConfig.removed = removed
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
        AutomationStore.resyncStored()
    }

    /// Demo modu acilip kapaninca (ya da ayarlar baska yoldan degisince) telefondaki kopyalar BridgeConfig'ten yeniden okunur.
    func reloadConfig() {
        names = BridgeConfig.names; icons = BridgeConfig.icons; roles = BridgeConfig.roles; removed = BridgeConfig.removed
        deletedAt = [:]; pendingUntil = [:]
        state = nil; error = nil
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
        Task { await refresh() }
    }

    func startPolling() {
        guard polling == nil else { return }     // acilista iki kez cagrilabilir; calisan yoklamayi iptal etme
        polling = Task { [weak self] in
            while !Task.isCancelled {
                await self?.refresh()
                try? await Task.sleep(nanoseconds: 2_000_000_000)
            }
        }
    }

    func stopPolling() { polling?.cancel(); polling = nil }

    func refresh() async {
        do {
            let client = try BridgeClient.fromConfig()
            var s = try await client.state()
            // Kisa sure once dokunulan isigin ekranda zipliyor gibi gorunmemesi icin: komut onaylanana kadar iyimser deger kalir.
            if let cur = state {
                for l in LightID.devices where (pendingUntil[l] ?? .distantPast) > Date() { s = s.setting(l, on: cur.isOn(l)) }
            }
            if s.legacy != BridgeConfig.legacy {                   // kopru turu degisti (genel <-> kisisel): yerlesik isiklar gorunur/gizlenir
                BridgeConfig.legacy = s.legacy
                removed = BridgeConfig.removed
                WidgetCenter.shared.reloadAllTimelines()
                NowlightShortcuts.updateAppShortcutParameters()
            }
            syncRoles(s)
            adoptBridgeDevices(s)
            state = s
            StateCache.save(s)
            error = nil
            lastUpdate = Date()
        } catch is CancellationError {
            // yoklama durduruldu: hata degil
        } catch {
            self.error = (error as? LocalizedError)?.errorDescription ?? "Bağlantı hatası"
        }
    }

    /// Kopru rolleri (servo, PIR) bildirir: telefondakini tazeler; gorunumler ve widget/Siri listeleri guncellenir.
    private func syncRoles(_ s: LightsState) {
        let fresh = s.roles.filter { $0.value != "light" }
        guard fresh != BridgeConfig.roles else { return }
        BridgeConfig.roles = fresh
        roles = fresh
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
    }

    /// Kopru "d3".."d6" ışığını biliyor ama uygulamada yoksa (eşleşme cevabı yarıda kaldıysa, başka telefondan eklendiyse) listeye ekler.
    private func adoptBridgeDevices(_ s: LightsState) {
        let known = Set(BridgeConfig.customDevices)
        let found = s.values.keys.filter {
            $0.count == 2 && $0.hasPrefix("d") && ($0.last?.isNumber ?? false) && !known.contains($0)
                && Date().timeIntervalSince(deletedAt[$0] ?? .distantPast) > 60      // yeni silinen isigi 60 sn geri ekleme
        }
        guard !found.isEmpty else { return }
        BridgeConfig.customDevices = BridgeConfig.customDevices + found.sorted()
        removed = BridgeConfig.removed          // gorunumler tazelensin
        names = BridgeConfig.names
        WidgetCenter.shared.reloadAllTimelines()
        NowlightShortcuts.updateAppShortcutParameters()
    }

    func toggle(_ light: LightID) async {
        let wantOn: Bool = {
            guard let s = state else { return true }
            return light == .all ? s.onCount == 0 : !s.isOn(light)
        }()
        if let s = state {
            state = s.setting(light, on: wantOn)
            let until = Date().addingTimeInterval(1.5)
            for l in (light == .all ? LightID.lights : [light]) { pendingUntil[l] = until }
        }
        do {
            try await BridgeClient.fromConfig().apply(light, on: wantOn)
            error = nil
            try? await Task.sleep(nanoseconds: 700_000_000)
            pendingUntil = [:]
            await refresh()
            WidgetCenter.shared.reloadAllTimelines()
        } catch {
            pendingUntil = [:]
            self.error = (error as? LocalizedError)?.errorDescription ?? "Komut gönderilemedi"
            await refresh()
        }
    }
}
