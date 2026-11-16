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
    private var pendingUntil: [LightID: Date] = [:]   // komut gonderilen isigin eski cevapla geri donmemesi icin

    var connected: Bool { state != nil && error == nil }

    /// Kaldirilan cihazlar (BridgeConfig ile esit tutulur; gorunumler bunu izler)
    var removed: Set<String> = BridgeConfig.removed

    /// Kullanici adlari (gorunumler bunu izler)
    var names: [String: String] = BridgeConfig.names

    /// Kullanici simgeleri
    var icons: [String: String] = BridgeConfig.icons

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
    func addDevice(_ light: LightID, name: String, symbol: String) {
        var list = BridgeConfig.customDevices
        if !list.contains(light.rawValue) { list.append(light.rawValue) }
        BridgeConfig.customDevices = list
        removed.remove(light.rawValue); BridgeConfig.removed = removed
        rename(light, to: name)
        setIcon(light, symbol)
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

    func toggle(_ light: LightID) async {
        let wantOn: Bool = {
            guard let s = state else { return true }
            return light == .all ? s.onCount == 0 : !s.isOn(light)
        }()
        if let s = state {
            state = s.setting(light, on: wantOn)
            let until = Date().addingTimeInterval(1.5)
            for l in (light == .all ? LightID.devices : [light]) { pendingUntil[l] = until }
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
