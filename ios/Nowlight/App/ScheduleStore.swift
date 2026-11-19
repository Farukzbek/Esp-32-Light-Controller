import SwiftUI

/// Zamanli programlar: gercek kaynak telefondur, her degisiklikte kopruye gonderilir (kapi kurallariyla ayni model).
@MainActor @Observable
final class ScheduleStore {
    enum Sync: Equatable { case unknown, synced, pending, oldBridge, offline }

    private static let key = "scheduleRules"
    private static var defaults: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }

    var rules: [ScheduleRule] = {
        guard let d = defaults.data(forKey: key), let r = try? JSONDecoder().decode([ScheduleRule].self, from: d) else { return [] }
        return r
    }()
    var info: ScheduleInfo?
    var sync: Sync = .unknown
    private var dirty = false

    /// Silinen bir cihazin yuvasi (bit) kayitli programlardan temizlenir; yuva sonradan baska cihaza verilirse eski program ona uygulanmasin.
    static func clearBit(_ bit: Int) {
        guard let d = defaults.data(forKey: key), var r = try? JSONDecoder().decode([ScheduleRule].self, from: d) else { return }
        for i in r.indices { r[i].devices &= ~bit }
        if let nd = try? JSONEncoder().encode(r) { defaults.set(nd, forKey: key) }
        Task { _ = try? await BridgeClient.fromConfig().pushSchedules(r) }
    }

    private func save() { if let d = try? JSONEncoder().encode(rules) { Self.defaults.set(d, forKey: Self.key) } }

    func changed() { save(); dirty = true; sync = .pending; Task { await refresh() } }
    func upsert(_ r: ScheduleRule) {
        if let i = rules.firstIndex(where: { $0.id == r.id }) { rules[i] = r } else { rules.append(r) }
        changed()
    }
    func delete(at o: IndexSet) { rules.remove(atOffsets: o); changed() }
    func setEnabled(_ id: UUID, _ on: Bool) { if let i = rules.firstIndex(where: { $0.id == id }) { rules[i].enabled = on; changed() } }

    /// Hazir sablonlar
    func addTemplate(_ name: String) {
        switch name {
        case "perde": rules.append(ScheduleRule(name: "Sabah perdeyi aç", action: .angle, devices: servoMask, minute: 7 * 60 + 30, days: 0b0111111, angle: 90))
        case "gece": rules.append(ScheduleRule(name: "Gece hepsini kapat", action: .off, devices: lightMask, minute: 23 * 60, days: 127))
        default: rules.append(ScheduleRule(name: "Akşam aç", action: .on, devices: lightMask, minute: 19 * 60, days: 127))
        }
        changed()
    }
    private var lightMask: Int { LightID.lights.reduce(0) { $0 | $1.bit } }
    private var servoMask: Int { LightID.devices.filter { $0.role == .servo }.reduce(0) { $0 | $1.bit } }

    /// Kopruyla esitle: telefonda program yoksa koprudekini al (yeni telefon); farkliysa telefondakini gonder.
    func refresh() async {
        guard BridgeConfig.isConfigured, let c = try? BridgeClient.fromConfig() else { sync = .offline; return }
        do {
            var i = try await c.schedules()
            if rules.isEmpty && !dirty && !i.rules.isEmpty {
                rules = ScheduleRule.parse(i.rules); save()
            } else if dirty || i.rules != rules.map(\.wire).joined(separator: ";") {
                i = try await c.pushSchedules(rules)
            }
            dirty = false
            info = i
            sync = .synced
        } catch BridgeError.badResponse { sync = .oldBridge }
        catch { sync = .offline }
    }
}
