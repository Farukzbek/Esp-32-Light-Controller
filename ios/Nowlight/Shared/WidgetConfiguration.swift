import AppIntents
import WidgetKit

/// Widget'a uzun basip "Widget'ı Düzenle" ile ayarlanir: hangi ışıklar (en fazla 4, sirasiyla) ve acik rengi.
struct LightsConfigurationIntent: WidgetConfigurationIntent {
    static var title: LocalizedStringResource = "Işıklar"
    static var description = IntentDescription("Widget'ta görünecek ışıkları ve açık rengini seç.")

    @Parameter(title: "1. ışık") var slot1: SlotEntity?
    @Parameter(title: "2. ışık") var slot2: SlotEntity?
    @Parameter(title: "3. ışık") var slot3: SlotEntity?
    @Parameter(title: "4. ışık") var slot4: SlotEntity?
    @Parameter(title: "5. ışık") var slot5: SlotEntity?
    @Parameter(title: "6. ışık") var slot6: SlotEntity?
    @Parameter(title: "7. ışık") var slot7: SlotEntity?
    @Parameter(title: "8. ışık") var slot8: SlotEntity?
    @Parameter(title: "Açık rengi", default: .amber) var accent: AccentChoice

    init() {}

    /// Secilmemis yuva konumuna gore varsayilan isigi gosterir (masa, yatak, led, tumu); "Bos" secilen yuva gosterilmez.
    var lights: [LightID] {
        let defaults: [LightID] = [.masa, .yatak, .led, .all]
        let slots = [slot1, slot2, slot3, slot4, slot5, slot6, slot7, slot8]
        return slots.enumerated().compactMap { i, s -> LightID? in
            // 1-4. yuva secilmediyse konumuna gore varsayilan; 5-8. yuva bos birakilirsa gosterilmez
            guard let l = s == nil ? (i < 4 ? defaults[i] : nil) : s?.light else { return nil }
            return l == .all ? (LightID.devices.count >= 2 ? l : nil) : (LightID.devices.contains(l) ? l : nil)
        }
    }
}
