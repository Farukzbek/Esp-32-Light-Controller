import SwiftUI

/// Servo elle kontrol: kaydirici ile 0...180 derece, hazir konumlar ve "birak" (servo gevser, elle oynatilabilir).
struct ServoSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model
    let light: LightID
    @State private var angle: Double = 90
    @State private var started = false

    private var info: LightsState.ServoInfo? { model.state?.servo[light.rawValue] }
    private var settings: ServoSettings { model.servoSettings(light) }

    var body: some View {
        NavigationStack {
            VStack(spacing: 22) {
                VStack(spacing: 4) {
                    TimelineView(.animation(minimumInterval: 0.1, paused: !(model.servoMoves[light.rawValue].map { $0.until > Date() } ?? false))) { _ in
                        let m = model.servoMoves[light.rawValue]
                        let live = (m.map { $0.until > Date() } ?? false) ? m!.angle() : Int(angle)       // hareket sirasinda canli aci, aksi halde kaydirici
                        Text("\(live)°").font(.system(size: 64, weight: .bold, design: .rounded)).monospacedDigit()
                    }
                    let moving = (model.servoMoves[light.rawValue]?.until ?? .distantPast) > Date() || info?.moving == true
                    Text(moving ? "Hareket ediyor…" : info?.free == true ? "Serbest: elle oynatılabilir" : "Tutuyor").font(.subheadline)
                        .foregroundStyle(moving || info?.free == true ? Color.lampOn : .secondary)
                }.padding(.top, 8)

                Slider(value: $angle, in: Double(settings.min)...Double(settings.max), step: 1) { editing in
                    if !editing { Task { await model.servoGoto(light, angle: Int(angle)) } }
                }.tint(Color.lampOn).padding(.horizontal, 24)

                HStack(spacing: 12) {
                    Button { angle = Double(settings.off); Task { await model.servoGoto(light, angle: settings.off) } } label: { Label("Kapalı \(settings.off)°", systemImage: "arrow.down.left.circle").frame(maxWidth: .infinity) }
                    Button { angle = Double(settings.on); Task { await model.servoGoto(light, angle: settings.on) } } label: { Label("Açık \(settings.on)°", systemImage: "arrow.up.right.circle.fill").frame(maxWidth: .infinity) }
                }.buttonStyle(.bordered).tint(Color.lampOn).controlSize(.large)

                Button { Task { await model.servoFree(light) } } label: { Label("Bırak (serbest)", systemImage: "hand.raised.fill").frame(maxWidth: .infinity) }
                    .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large)

                Text("Servo hedefe varınca ayarlı süre sonra kendiliğinden bırakılır ve elle oynatılabilir, karşı koymaz. Konum geri okunmaz: bildirilen açı son komut verilen açıdır.")
                    .font(.footnote).foregroundStyle(.secondary).multilineTextAlignment(.center).padding(.horizontal, 20)
                Spacer()
            }
            .navigationTitle(light.title)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Bitti") { dismiss() } } }
            .onAppear { if !started, let a = info?.angle, a >= 0 { angle = Double(a) }; started = true }
        }
        .presentationDetents([.medium, .large])
        .preferredColorScheme(.dark)
    }
}

/// PIR sensoru karti: ac/kapat yok, sadece hareket durumu. Hareket varken sicak parlar.
struct PirTile: View {
    @Environment(AppModel.self) private var model
    let light: LightID

    private var moving: Bool { model.state?.value(light) == 1 }
    private var known: Bool { (model.state?.value(light) ?? -1) != -1 }

    var body: some View {
        let _ = (model.names, model.icons, model.roles)
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Image(systemName: light.symbol).font(.system(size: 26, weight: .semibold)).symbolEffect(.pulse, isActive: moving)
                Spacer()
                Circle().fill(moving ? Color.black.opacity(0.35) : Color.white.opacity(0.14)).frame(width: 14, height: 14)
            }
            Spacer(minLength: 18)
            Text(light.title).font(.system(size: 17, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.8)
            Text(!known ? "—" : moving ? "Hareket var" : "Hareket yok").font(.system(size: 14, weight: .medium))
                .foregroundStyle(moving ? Color(hex: 0x14120C) : .white).opacity(moving ? 0.7 : 0.55)
        }
        .padding(16)
        .frame(maxWidth: .infinity, minHeight: 150, alignment: .topLeading)
        .foregroundStyle(moving ? Color(hex: 0x14120C) : .white)
        .background(moving ? AnyShapeStyle(Color.lampOn) : AnyShapeStyle(Color.white.opacity(0.09)), in: RoundedRectangle(cornerRadius: 22, style: .continuous))
        .animation(.easeInOut(duration: 0.25), value: moving)
    }
}

/// Gun secici (Pzt ... Paz). bit0 = Pazartesi ... bit6 = Pazar.
struct DayPicker: View {
    @Binding var days: Int
    private static var names: [String] { AutomationRule.dayNames }
    private static let full = ["Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi", "Pazar"]

    var body: some View {
        HStack(spacing: 6) {
            ForEach(0..<7, id: \.self) { i in
                let on = days & (1 << i) != 0
                Button { toggle(i) } label: {
                    Text(Self.names[i]).font(.system(size: 13, weight: .semibold)).frame(maxWidth: .infinity).frame(height: 38)
                        .foregroundStyle(on ? .black : .white)
                        .background(on ? Color.lampOn : Color.white.opacity(0.10), in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                }.buttonStyle(.plain).accessibilityLabel(Self.full[i]).accessibilityAddTraits(on ? .isSelected : [])
            }
        }
    }

    /// En az bir gun secili kalir (hic gun = kural asla calismaz).
    private func toggle(_ i: Int) {
        let next = days ^ (1 << i)
        if next & 127 != 0 { days = next & 127 }
    }
}
