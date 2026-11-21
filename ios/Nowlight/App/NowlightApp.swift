import SwiftUI

@main
struct NowlightApp: App {
    @State private var model = AppModel()

    init() { DemoMode.register(); NotificationService.register() }

    var body: some Scene {
        WindowGroup {
            RootView()
                .environment(model)
                .preferredColorScheme(.dark)
        }
    }
}
