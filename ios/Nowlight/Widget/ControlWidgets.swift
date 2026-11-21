import AppIntents
import SwiftUI
import WidgetKit

// Denetim Merkezi / Kilit ekrani / Action Button dugmeleri (iOS 18+).
// Action Button'a atamak icin: Ayarlar > Action Button > Kontroller > "Nowlight Işık" ya da "Nowlight Tüm ışıklar".

@available(iOS 18.0, *)
struct LightControlConfiguration: ControlConfigurationIntent {
    static var title: LocalizedStringResource = "Işık"
    static var description = IntentDescription("Denetim Merkezi'nde açıp kapatacağın ışık.")
    @Parameter(title: "Işık") var light: LightEntity?
}

/// Denetim Merkezi anahtari bu intent'i calistirir: hedef durum sistemden gelir (value).
@available(iOS 18.0, *)
struct SetLightValueIntent: SetValueIntent {
    static var title: LocalizedStringResource = "Işığı ayarla"
    @Parameter(title: "Işık") var light: LightEntity
    @Parameter(title: "Açık") var value: Bool

    init() { light = LightEntity(.masa); value = false }
    init(light: LightID) { self.light = LightEntity(light); value = false }

    func perform() async throws -> some IntentResult {
        try await LightActions.set(light.light, on: value)
        return .result()
    }
}

@available(iOS 18.0, *)
struct LightControl: ControlWidget {
    static let kind = "online.matestudios.nowlight.control.light"

    struct Value { var isOn: Bool; var light: LightID }

    struct Provider: AppIntentControlValueProvider {
        func previewValue(configuration: LightControlConfiguration) -> Value {
            Value(isOn: false, light: configuration.light?.light ?? LightID.controllable.first ?? .masa)
        }
        func currentValue(configuration: LightControlConfiguration) async throws -> Value {
            let l = configuration.light?.light ?? LightID.controllable.first ?? .masa
            let s = try? await BridgeClient.fromConfig().state()
            return Value(isOn: s?.isOn(l) ?? false, light: l)
        }
    }

    var body: some ControlWidgetConfiguration {
        AppIntentControlConfiguration(kind: Self.kind, provider: Provider()) { value in
            ControlWidgetToggle(isOn: value.isOn, action: SetLightValueIntent(light: value.light)) {
                Label(value.light.title, systemImage: value.light.symbol)
            } valueLabel: { isOn in
                Text(isOn ? "Açık" : "Kapalı")
            }
        }
        .displayName("Nowlight Işık")
        .description("Seçtiğin ışığı Denetim Merkezi'nden, kilit ekranından ya da Action Button'dan aç/kapat.")
    }
}

/// "Tum isiklar" anahtari (yapilandirma gerektirmez).
@available(iOS 18.0, *)
struct AllLightsControl: ControlWidget {
    static let kind = "online.matestudios.nowlight.control.all"

    struct Provider: ControlValueProvider {
        var previewValue: Bool { false }
        func currentValue() async throws -> Bool { (try? await BridgeClient.fromConfig().state())?.onCount ?? 0 > 0 }
    }

    var body: some ControlWidgetConfiguration {
        StaticControlConfiguration(kind: Self.kind, provider: Provider()) { anyOn in
            ControlWidgetToggle(isOn: anyOn, action: SetLightValueIntent(light: .all)) {
                Label("Tüm ışıklar", systemImage: "lightswitch.on.fill")
            } valueLabel: { isOn in
                Text(isOn ? "Açık" : "Kapalı")
            }
        }
        .displayName("Nowlight Tüm ışıklar")
        .description("Tüm ışıkları tek dokunuşla aç/kapat.")
    }
}
