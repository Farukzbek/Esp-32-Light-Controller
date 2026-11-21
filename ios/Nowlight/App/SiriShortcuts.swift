import AppIntents

/// Siri ve Kisayollar uygulamasi icin hazir komutlar. Isik adlari dinamiktir (kullanicinin verdigi ad).
struct TurnLightOnIntent: AppIntent {
    static var title: LocalizedStringResource = "Işığı aç"
    static var description = IntentDescription("Seçilen ışığı açar.")
    static var openAppWhenRun = false
    @Parameter(title: "Işık") var light: LightEntity
    static var parameterSummary: some ParameterSummary { Summary("\(\.$light) aç") }

    func perform() async throws -> some IntentResult & ProvidesDialog {
        try await LightActions.set(light.light, on: true)
        return .result(dialog: "\(light.light.title) açıldı")
    }
}

struct TurnLightOffIntent: AppIntent {
    static var title: LocalizedStringResource = "Işığı kapat"
    static var description = IntentDescription("Seçilen ışığı kapatır.")
    static var openAppWhenRun = false
    @Parameter(title: "Işık") var light: LightEntity
    static var parameterSummary: some ParameterSummary { Summary("\(\.$light) kapat") }

    func perform() async throws -> some IntentResult & ProvidesDialog {
        try await LightActions.set(light.light, on: false)
        return .result(dialog: "\(light.light.title) kapatıldı")
    }
}

struct AllLightsOnIntent: AppIntent {
    static var title: LocalizedStringResource = "Tüm ışıkları aç"
    static var openAppWhenRun = false
    func perform() async throws -> some IntentResult & ProvidesDialog {
        try await LightActions.set(.all, on: true)
        return .result(dialog: "Tüm ışıklar açıldı")
    }
}

struct AllLightsOffIntent: AppIntent {
    static var title: LocalizedStringResource = "Tüm ışıkları kapat"
    static var openAppWhenRun = false
    func perform() async throws -> some IntentResult & ProvidesDialog {
        try await LightActions.set(.all, on: false)
        return .result(dialog: "Tüm ışıklar kapatıldı")
    }
}

struct NowlightShortcuts: AppShortcutsProvider {
    static var appShortcuts: [AppShortcut] {
        AppShortcut(intent: AllLightsOnIntent(),
                    phrases: ["\(.applicationName) tüm ışıkları aç", "\(.applicationName) ile tüm ışıkları aç"],
                    shortTitle: "Tüm ışıkları aç", systemImageName: "lightbulb.max.fill")
        AppShortcut(intent: AllLightsOffIntent(),
                    phrases: ["\(.applicationName) tüm ışıkları kapat", "\(.applicationName) ile tüm ışıkları kapat"],
                    shortTitle: "Tüm ışıkları kapat", systemImageName: "lightbulb.slash.fill")
        AppShortcut(intent: RunSceneIntent(),
                    phrases: ["\(.applicationName) \(\.$scene) sahnesini çalıştır", "\(.applicationName) \(\.$scene) sahnesi"],
                    shortTitle: "Sahneyi çalıştır", systemImageName: "sparkles")
        AppShortcut(intent: TurnLightOnIntent(),
                    phrases: ["\(.applicationName) \(\.$light) aç", "\(.applicationName) ile \(\.$light) aç"],
                    shortTitle: "Işığı aç", systemImageName: "lightbulb.fill")
        AppShortcut(intent: TurnLightOffIntent(),
                    phrases: ["\(.applicationName) \(\.$light) kapat", "\(.applicationName) ile \(\.$light) kapat"],
                    shortTitle: "Işığı kapat", systemImageName: "lightbulb.slash")
    }
}
