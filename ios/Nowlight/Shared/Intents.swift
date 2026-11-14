import AppIntents
import WidgetKit

/// Widget dugmesi ve Kisayollar/Siri icin: bir isigi tersine cevirir (acik ise kapatir, degilse acar).
/// Uygulama acilmadan calisir; kopruya kisa bir yerel ag istegi atar.
struct ToggleLightIntent: AppIntent {
    static var title: LocalizedStringResource = "Işığı aç/kapat"
    static var description = IntentDescription("Seçilen ışığı tersine çevirir.")
    static var openAppWhenRun = false

    @Parameter(title: "Işık") var light: LightID

    init() { light = .masa }
    init(light: LightID) { self.light = light }

    func perform() async throws -> some IntentResult {
        let client = try BridgeClient.fromConfig()
        let state = try? await client.state()
        let wantOn = (state?.value(light) ?? 0) != 1      // acik ise kapat; kapali/bilinmiyorsa ac
        try await client.apply(light, on: wantOn)
        try? await Task.sleep(nanoseconds: 900_000_000)   // dugum onaylasin, widget dogru durumu okusun
        WidgetCenter.shared.reloadAllTimelines()
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
        try await BridgeClient.fromConfig().apply(.all, on: on)
        try? await Task.sleep(nanoseconds: 900_000_000)
        WidgetCenter.shared.reloadAllTimelines()
        return .result()
    }
}
