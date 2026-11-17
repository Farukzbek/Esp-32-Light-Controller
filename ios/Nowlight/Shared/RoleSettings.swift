import Foundation

/// Servo ayarlari. Kopruye ve oradan dugume gider; telefonda da saklanir (App Group), dugum geri okunmaz.
/// hiz: 10 derece/sn biriminde (0 = aninda). birakma: 100 ms biriminde (0 = asla birakma, surekli tut). Servo hedefe varinca birakma suresi sonra
/// serbest kalir ve elle oynatilabilir (karsi koymaz).
struct ServoSettings: Equatable {
    var pin = 5
    var inv = false
    var off = 0
    var on = 90
    var speed = 0
    var release = 10
    var min = 0
    var max = 180

    var stored: String { "\(pin):\(inv ? 1 : 0):\(off):\(on):\(speed):\(release):\(min):\(max)" }

    init(pin: Int = 5, inv: Bool = false, off: Int = 0, on: Int = 90, speed: Int = 0, release: Int = 10, min: Int = 0, max: Int = 180) {
        self.pin = pin; self.inv = inv; self.off = off; self.on = on; self.speed = speed; self.release = release; self.min = min; self.max = max
    }

    init?(stored: String?) {
        guard let v = stored?.split(separator: ":").compactMap({ Int($0) }), v.count == 8 else { return nil }
        self.init(pin: v[0], inv: v[1] != 0, off: v[2], on: v[3], speed: v[4], release: v[5], min: v[6], max: v[7])
    }
}

/// PIR ayarlari: tutma (sn, hareket bittikten sonra "hareket var" kalma suresi), onay (10 ms: bundan kisa darbeler yok sayilir),
/// bekleme (sn, temizlendikten sonra yeniden tetikleme yok), ters (LOW = hareket).
/// Algilama acisi ve menzili cihazin lensi ve potansiyometresidir; yazilimdan degismez.
struct PirSettings: Equatable {
    var pin = 4
    var hold = 5
    var confirm = 5
    var cooldown = 0
    var inv = false

    var stored: String { "\(pin):\(hold):\(confirm):\(cooldown):\(inv ? 1 : 0)" }

    init(pin: Int = 4, hold: Int = 5, confirm: Int = 5, cooldown: Int = 0, inv: Bool = false) {
        self.pin = pin; self.hold = hold; self.confirm = confirm; self.cooldown = cooldown; self.inv = inv
    }

    init?(stored: String?) {
        guard let v = stored?.split(separator: ":").compactMap({ Int($0) }), v.count == 5 else { return nil }
        self.init(pin: v[0], hold: v[1], confirm: v[2], cooldown: v[3], inv: v[4] != 0)
    }
}
