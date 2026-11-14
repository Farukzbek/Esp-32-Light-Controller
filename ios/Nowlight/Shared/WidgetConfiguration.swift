import AppIntents
import WidgetKit

/// Widget'a uzun basip "Widget'ı Düzenle" ile ayarlanir: hangi ışıklar (en fazla 4, sirasiyla) ve acik rengi.
struct LightsConfigurationIntent: WidgetConfigurationIntent {
    static var title: LocalizedStringResource = "Işıklar"
    static var description = IntentDescription("Widget'ta görünecek ışıkları ve açık rengini seç.")

    @Parameter(title: "1. ışık", default: .masa) var slot1: SlotChoice
    @Parameter(title: "2. ışık", default: .yatak) var slot2: SlotChoice
    @Parameter(title: "3. ışık", default: .led) var slot3: SlotChoice
    @Parameter(title: "4. ışık", default: .all) var slot4: SlotChoice
    @Parameter(title: "Açık rengi", default: .amber) var accent: AccentChoice

    init() {}

    var lights: [LightID] {
        [slot1, slot2, slot3, slot4].compactMap(\.light).filter { $0 == .all ? LightID.devices.count >= 2 : LightID.devices.contains($0) }
    }
}
