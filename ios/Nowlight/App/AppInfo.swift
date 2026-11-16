import Foundation

enum AppInfo {
    /// Projenin acik kaynak deposu (ESP32 yazilimi, kopru ve bu uygulama)
    static let repoURL = URL(string: "https://github.com/Farukzbek/Esp-32-Light-Controller")!

    /// ESP32 yazilimini tarayicidan yukleyen web yukleyici (bilgisayarda Chrome/Edge)
    static let flashURL = URL(string: "https://flash.matestudios.online")!

    /// Kurulum rehberi (GitHub'daki belgeler)
    static let docsURL = URL(string: "https://github.com/Farukzbek/Esp-32-Light-Controller/blob/main/docs/setup.md")!

    static var version: String {
        let v = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "1.0"
        let b = Bundle.main.object(forInfoDictionaryKey: "CFBundleVersion") as? String ?? "1"
        return "\(v) (\(b))"
    }
}
