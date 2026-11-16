import AppIntents
import WidgetKit

/// Komut gonderme akisi (widget ve Kisayollar ortak): once beklenen durumu hemen widget'a yansit (iyimser),
/// sonra kopruya tek istek at, kisa sure sonra gercek durumla tekrar yenile. Onceden durum sormaz.
enum LightActions {
    /// Komutu gonderip HEMEN doner: iOS widget'i ancak perform() bitince yeniler, bu yuzden beklemek gorunumu geciktirir.
    /// Once beklenen durumu onbellege yazariz; widget onu aninda gosterir, 2 sn sonra gercek durumu okur (LightsProvider).
    static func set(_ light: LightID, on: Bool) async throws {
        let client = try BridgeClient.fromConfig()
        let previous = StateCache.load()
        if let p = previous { StateCache.save(p.state.setting(light, on: on), optimistic: true) }
        do {
            try await client.apply(light, on: on)
        } catch {
            if let p = previous { StateCache.save(p.state, optimistic: false) }   // basarisiz: eski duruma don
            throw error
        }
        WidgetCenter.shared.reloadAllTimelines()
    }
}

/// Widget dugmesi ve Kisayollar: bir isigi tersine cevirir. Hedef, CALISIRKEN en guncel duruma gore hesaplanir:
/// az once yapilan basis (iyimser onbellek) varsa ona, yoksa kopruya sorarak. Boylece hizli art arda basislar dogru calisir.
struct ToggleLightIntent: AppIntent {
    static var title: LocalizedStringResource = "Işığı aç/kapat"
    static var description = IntentDescription("Seçilen ışığı tersine çevirir.")
    static var openAppWhenRun = false

    @Parameter(title: "Işık") var light: LightEntity

    init() { light = LightEntity(.masa) }
    init(light: LightID) { self.light = LightEntity(light) }

    func perform() async throws -> some IntentResult {
        let cached = StateCache.load()
        let base: LightsState?
        if let c = cached, c.optimistic, Date().timeIntervalSince(c.at) < 3 {
            base = c.state                                                   // az once basildi: o beklenen durumdan devam et
        } else {
            base = (try? await BridgeClient.fromConfig().state()) ?? cached?.state
        }
        let target = light.light
        let wantOn: Bool = target == .all ? (base?.onCount ?? 0) == 0 : (base?.value(target) ?? 0) != 1
        try await LightActions.set(target, on: wantOn)
        return .result()
    }
}

/// Kisayollar/Siri icin: tum isiklari ac ya da kapat.
struct SetAllLightsIntent: AppIntent {
    static var title: LocalizedStringResource = "Tüm ışıkları ayarla"
    static var openAppWhenRun = false

    @Parameter(title: "Açık") var on: Bool

    init() { on = false }
    init(on: Bool) { self.on = on }

    func perform() async throws -> some IntentResult {
        try await LightActions.set(.all, on: on)
        return .result()
    }
}
