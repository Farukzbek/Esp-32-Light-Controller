import SwiftUI

/// Otomasyon kurallari: gercek kaynak telefondur, her degisiklikte kapi dugumune gonderilir.
/// Dugum yazilimi eski ya da ulasilamiyorsa kurallar telefonda bekler, sonraki baglantida gonderilir.
@MainActor @Observable
final class AutomationStore {
    enum Sync: Equatable { case unknown, synced, pending, oldFirmware, offline }

    private static let key = "doorRules"
    private static var defaults: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }

    var rules: [AutomationRule] = {
        guard let d = defaults.data(forKey: key), let r = try? JSONDecoder().decode([AutomationRule].self, from: d) else { return [] }
        return r
    }()
    var status: DoorStatus?
    var sync: Sync = .unknown
    var message: String?
    private var dirty = true          // acilista bir kere dugumle esitle
    private var poll: Task<Void, Never>?

    var client: DoorClient { DoorClient(host: DoorConfig.host, token: DoorConfig.token) }

    /// Kaldirilan/geri eklenen isiklar kurallari etkiler: telefondaki kurallari arka planda dugume yeniden gonderir.
    static func resyncStored() {
        guard DoorConfig.isConfigured, let d = defaults.data(forKey: key), let r = try? JSONDecoder().decode([AutomationRule].self, from: d), !r.isEmpty else { return }
        Task { _ = try? await DoorClient(host: DoorConfig.host, token: DoorConfig.token).pushRules(r) }
    }

    private func save() {
        if let d = try? JSONEncoder().encode(rules) { Self.defaults.set(d, forKey: Self.key) }
    }

    func changed() { save(); UserDefaults.standard.set(true, forKey: "doorRulesEdited"); dirty = true; sync = .pending; Task { await trySync() } }

    func upsert(_ r: AutomationRule) {
        if let i = rules.firstIndex(where: { $0.id == r.id }) { rules[i] = r } else { rules.append(r) }
        changed()
    }
    func delete(at o: IndexSet) { rules.remove(atOffsets: o); changed() }
    func move(from: IndexSet, to: Int) { rules.move(fromOffsets: from, toOffset: to); changed() }
    func setEnabled(_ id: UUID, _ on: Bool) { if let i = rules.firstIndex(where: { $0.id == id }) { rules[i].enabled = on; changed() } }

    /// Hazir sablon: gece ac, gunduz kapat.
    func addDayNightTemplate() {
        rules += [
            AutomationRule(name: "Akşam aç", enabled: true, action: .on, lamps: 7, start: 18 * 60, end: 6 * 60, cooldownMin: 0),
            AutomationRule(name: "Gündüz kapat", enabled: true, action: .off, lamps: 7, start: 6 * 60, end: 18 * 60, cooldownMin: 0),
        ]
        changed()
    }

    func trySync() async {
        guard DoorConfig.isConfigured else { return }
        guard dirty else { return }
        do {
            let s = try await client.pushRules(rules)
            status = s; dirty = false; sync = .synced
        } catch BridgeError.badResponse {
            sync = .oldFirmware          // eski yazilimda /api/rules yok
        } catch is CancellationError {
        } catch { sync = .offline }
    }

    func reload() async {
        guard DoorConfig.isConfigured else { return }
        do {
            let s = try await client.status()
            status = s
            if s.rules == nil { sync = .oldFirmware; return }
            if rules.isEmpty && !hasEdited, (s.rules ?? 0) > 0 {      // telefonda hic kural yok: dugumdekini al
                rules = try await client.pullRules(); save(); dirty = false; sync = .synced
            } else if dirty { await trySync() }
            else if sync != .synced { sync = .synced }
        } catch is CancellationError {
        } catch { sync = .offline }
    }
    private var hasEdited: Bool { UserDefaults.standard.bool(forKey: "doorRulesEdited") }

    func start() {
        poll?.cancel()
        poll = Task { while !Task.isCancelled { await reload(); try? await Task.sleep(for: .seconds(2)) } }
    }
    func stop() { poll?.cancel() }

    func setMaster(_ on: Bool) async {
        do { status = try await client.configure(["enabled": on ? "1" : "0"]); message = nil }
        catch { message = (error as? LocalizedError)?.errorDescription ?? "Kaydedilemedi." }
    }
}
