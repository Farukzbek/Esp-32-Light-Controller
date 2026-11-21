import Foundation

/// Uygulamanin bildigi en yeni surumler. Kisisel (USB ile yuklenen) kopru ve kapi dugumu icin: cihaz kendi surumunu bildirir,
/// bu degerden eskiyse "guncelle" rozeti cikar. Her yeni ozellikli yazilimda bu sabitler yukseltilir.
/// Not: masa ve yatak dugumleri surum bildirmez (eski protokol), bu yuzden denetlenemez.
enum FirmwareCatalog {
    static let bridgePersonal = "0.7.0"
    static let door = "1.2"

    /// "0.7.0-dev" gibi son ekler yok sayilir. a, b'den eski mi?
    static func isOlder(_ a: String, than b: String) -> Bool {
        func parts(_ s: String) -> [Int] { s.split(separator: "-").first.map { $0.split(separator: ".").map { Int($0) ?? 0 } } ?? [] }
        let x = parts(a), y = parts(b)
        for i in 0..<max(x.count, y.count) {
            let p = i < x.count ? x[i] : 0, q = i < y.count ? y[i] : 0
            if p != q { return p < q }
        }
        return false
    }
}

struct FirmwareIssue: Identifiable, Hashable {
    let id: String
    let title: String
    let detail: String
}

enum FirmwareCheck {
    static func run() async -> [FirmwareIssue] {
        var out: [FirmwareIssue] = []
        if BridgeConfig.isConfigured, let c = try? BridgeClient.fromConfig() {
            do {
                let v = try await c.version()
                if v.generic {
                    if v.ota, let u = try? await c.updateStatus(), u.available {
                        out.append(FirmwareIssue(id: "bridge", title: "Köprü: \(u.latest) sürümü hazır", detail: "Şu an \(u.current). Cihazlar → Köprü ayarları → Köprü yazılımı bölümünden güncelle."))
                    }
                } else if FirmwareCatalog.isOlder(v.version, than: FirmwareCatalog.bridgePersonal) {
                    out.append(FirmwareIssue(id: "bridge", title: "Köprü yazılımı eski (\(v.version))", detail: "Yeni sürüm \(FirmwareCatalog.bridgePersonal). Köprüyü USB ile bilgisayara takıp yazılımı güncelle."))
                }
            } catch BridgeError.badResponse {
                out.append(FirmwareIssue(id: "bridge", title: "Köprü yazılımı eski", detail: "Programlar, zamanlayıcı ve geçmiş için köprüyü USB ile güncelle (yeni sürüm \(FirmwareCatalog.bridgePersonal))."))
            } catch { }
        }
        if DoorConfig.isConfigured, let s = try? await DoorClient(host: DoorConfig.host, token: DoorConfig.token).status() {
            if let fw = s.fw {
                if FirmwareCatalog.isOlder(fw, than: FirmwareCatalog.door) { out.append(FirmwareIssue(id: "door", title: "Kapı düğümü yazılımı eski (\(fw))", detail: "Yeni sürüm \(FirmwareCatalog.door). Kapı düğümünü USB ile güncelle.")) }
            } else {
                out.append(FirmwareIssue(id: "door", title: "Kapı düğümü yazılımı eski", detail: "Gir-çık, gün seçimi ve geçmiş için kapı düğümünü USB ile güncelle (yeni sürüm \(FirmwareCatalog.door))."))
            }
        }
        return out
    }
}
