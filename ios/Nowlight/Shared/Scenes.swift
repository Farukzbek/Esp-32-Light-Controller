import AppIntents
import Foundation
import SwiftUI
import WidgetKit

/// Sahne: birkac cihazi tek dokunusla belirli duruma getirir ("Aksam", "Yatmadan once"). Telefonda saklanir (App Group), calistirma
/// telefondan cihazlara tek tek komut gonderir (uygulama, Siri, Denetim Merkezi). Telefon ev agindayken calisir.
struct SceneStep: Codable, Equatable, Hashable {
    enum Mode: String, Codable, CaseIterable { case skip, on, off, angle
        var title: String { switch self { case .skip: "Dokunma"; case .on: "Aç"; case .off: "Kapat"; case .angle: "Açıya git" } }
    }
    var device: String            // LightID.rawValue
    var mode: Mode = .skip
    var angle = 90
}

struct SceneDef: Codable, Identifiable, Equatable {
    var id = UUID()
    var name = "Yeni sahne"
    var symbol = "sparkles"
    var steps: [SceneStep] = []

    /// Yalniz gercekten bir sey yapan adimlar, gorunen (silinmemis) cihazlar icin.
    var activeSteps: [SceneStep] {
        let valid = Set(LightID.controllable.map(\.rawValue))
        return steps.filter { $0.mode != .skip && valid.contains($0.device) }
    }
    var summary: String {
        let n = activeSteps.count
        return n == 0 ? "Henüz ayar yok" : activeSteps.prefix(3).compactMap { s in LightID(rawValue: s.device).map { "\($0.shortTitle) \(s.mode == .angle ? "\(s.angle)°" : s.mode.title.lowercased())" } }.joined(separator: " · ") + (n > 3 ? " +\(n - 3)" : "")
    }
}

enum SceneStore {
    private static let key = "scenes"
    private static var defaults: UserDefaults { UserDefaults(suiteName: BridgeConfig.group) ?? .standard }
    static func load() -> [SceneDef] {
        guard let d = defaults.data(forKey: key), let s = try? JSONDecoder().decode([SceneDef].self, from: d) else { return [] }
        return s
    }
    static func save(_ s: [SceneDef]) { if let d = try? JSONEncoder().encode(s) { defaults.set(d, forKey: key) } }
}

enum SceneRunner {
    /// Sahnedeki adimlari ayni anda uygular. Bir cihaz basarisiz olursa digerleri yine uygulanir; ilk hata firlatilir.
    static func run(_ scene: SceneDef) async throws {
        let client = try BridgeClient.fromConfig()
        let steps = scene.activeSteps
        var firstError: Error?
        await withTaskGroup(of: Error?.self) { g in
            for s in steps {
                guard let light = LightID(rawValue: s.device) else { continue }
                g.addTask {
                    do {
                        switch s.mode {
                        case .on: try await client.set(light, on: true)
                        case .off: try await client.set(light, on: false)
                        case .angle: try await client.servoGoto(id: light.rawValue, angle: s.angle)
                        case .skip: break
                        }
                        return nil
                    } catch { return error }
                }
            }
            for await e in g { if firstError == nil, let e { firstError = e } }
        }
        WidgetCenter.shared.reloadAllTimelines()
        if let firstError { throw firstError }
    }
}

// MARK: Siri / Kisayollar / Denetim Merkezi icin

struct SceneEntity: AppEntity {
    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Sahne" }
    static var defaultQuery = SceneEntityQuery()
    var id: String
    var scene: SceneDef? { SceneStore.load().first { $0.id.uuidString == id } }
    var displayRepresentation: DisplayRepresentation {
        DisplayRepresentation(title: "\(scene?.name ?? "Sahne")", image: .init(systemName: scene?.symbol ?? "sparkles"))
    }
    init(_ s: SceneDef) { id = s.id.uuidString }
}

struct SceneEntityQuery: EntityQuery {
    func entities(for identifiers: [String]) async throws -> [SceneEntity] { SceneStore.load().filter { identifiers.contains($0.id.uuidString) }.map(SceneEntity.init) }
    func suggestedEntities() async throws -> [SceneEntity] { SceneStore.load().map(SceneEntity.init) }
}

struct RunSceneIntent: AppIntent {
    static var title: LocalizedStringResource = "Sahneyi çalıştır"
    static var description = IntentDescription("Seçilen sahneyi uygular.")
    static var openAppWhenRun = false
    @Parameter(title: "Sahne") var scene: SceneEntity

    init() {}
    init(scene: SceneEntity) { self.scene = scene }
    static var parameterSummary: some ParameterSummary { Summary("\(\.$scene) sahnesini çalıştır") }

    func perform() async throws -> some IntentResult & ProvidesDialog {
        guard let s = scene.scene else { return .result(dialog: "Sahne bulunamadı") }
        try await SceneRunner.run(s)
        return .result(dialog: "\(s.name) uygulandı")
    }
}
