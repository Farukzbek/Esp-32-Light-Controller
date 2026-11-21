import SwiftUI

/// Alt sekme cubugu: Isiklar | Widgetlar | Cihaz Ekle
struct RootView: View {
    @Environment(AppModel.self) private var model
    @Environment(\.scenePhase) private var phase
    @State private var selection = UserDefaults.standard.integer(forKey: "tab")   // gelistirme: -tab 1
    @State private var showSettings = false
    @State private var showWelcome = false

    var body: some View {
        TabView(selection: $selection) {
            LightsScreen(showSettings: $showSettings)
                .tabItem { Label("Işıklar", systemImage: "lightbulb.fill") }.tag(0)
            AutomationHubScreen()
                .tabItem { Label("Otomasyon", systemImage: "wand.and.stars") }.tag(1)
            WidgetsScreen()
                .tabItem { Label("Widgetlar", systemImage: "square.grid.2x2.fill") }.tag(2)
            AddDeviceScreen(showSettings: $showSettings)
                .tabItem { Label("Cihazlar", systemImage: "plus.circle.fill") }.badge(model.firmwareIssues.count).tag(3)
        }
        .tint(Color.lampOn)
        .sheet(isPresented: $showSettings) { SettingsView() }
        // Ilk acilista GitHub'i tanitan pencere; kapaninca (kopru tanimli degilse) kopru ayarlari acilir.
        .sheet(isPresented: $showWelcome, onDismiss: {
            UserDefaults.standard.set(true, forKey: "welcomeSeen")
            if !BridgeConfig.isConfigured { showSettings = true }
        }) { WelcomeView() }
        .onChange(of: phase) { _, p in
            if p == .active { model.startPolling() } else { model.stopPolling() }
        }
        .onAppear {
            model.startPolling()
            Task { await model.checkFirmware(force: true) }
            if !UserDefaults.standard.bool(forKey: "welcomeSeen") { showWelcome = true }
            else if !BridgeConfig.isConfigured { showSettings = true }
        }
    }
}

/// Ana ekran: yalnizca isik dugmeleri. Baglanti sorunu olursa ustte kisa bir uyari cikar.
struct LightsScreen: View {
    @Environment(AppModel.self) private var model
    @Binding var showSettings: Bool
    @State private var showEditor = false

    private let columns = [GridItem(.flexible(), spacing: 14), GridItem(.flexible(), spacing: 14)]

    private var showAll: Bool { _ = (model.removed, model.homeHidden); return LightID.lights.count >= 2 && !model.homeHidden.contains("all") }
    private var showSiri: Bool { !model.homeHidden.contains("siri") }

    private var tiles: [LightID] {
        _ = (model.removed, model.roles, model.homeOrder, model.homeHidden)     // kaldirma, rol, sira ve gizleme degisimi gorunumu tazeler
        return model.homeDevices()
    }

    private var warning: String? {
        if !BridgeConfig.isConfigured { return nil }          // yerine "Once koprüyü bagla" karti gosterilir
        if let e = model.error { return e }
        guard let s = model.state else { return nil }
        _ = (model.names, model.removed)
        let down = LightID.devices.filter { !s.linked($0) }
        if down.isEmpty { return nil }
        return down.count == LightID.devices.count && down.count > 1 ? "Hiçbir düğüme ulaşılamıyor" : down.map(\.title).joined(separator: ", ") + " ulaşılamıyor"
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 14) {
                    if DemoMode.isOn { DemoBanner() }
                    if !BridgeConfig.isConfigured { BridgeConnectCard() }
                    if let w = warning { WarningCard(message: w, retry: model.error != nil) }
                    if tiles.isEmpty && !showAll {
                        ContentUnavailableView(LightID.devices.isEmpty ? "Cihaz yok" : "Kartlar gizli", systemImage: "lightbulb.slash",
                                               description: Text(LightID.allDevices.isEmpty ? "Cihaz Ekle sekmesinden ilk cihazını ekle: bir ışık, servo ya da hareket sensörü."
                                                                 : LightID.devices.isEmpty ? "Cihaz Ekle sekmesinden kaldırdığın cihazları geri ekleyebilirsin."
                                                                 : "Tüm kartları gizledin. Sol üstteki Düzenle ile geri aç."))
                            .padding(.top, 60)
                        if showSiri { SiriPromoCard().padding(.top, 10) }
                    } else {
                        if !model.scenes.isEmpty && !model.homeHidden.contains("scenes") { SceneChips() }
                        if showAll { LightTile(light: .all, wide: true) }      // "Tum isiklar": en ustte tam genislikte
                        LazyVGrid(columns: columns, spacing: 14) {
                            ForEach(tiles, id: \.self) { l in
                                if l.role == .pir { PirTile(light: l) } else { LightTile(light: l) }
                            }
                        }
                        if showSiri { SiriPromoCard().padding(.top, 10) }
                    }
                }
                .padding(.horizontal, 18)
                .padding(.top, 6)
                .padding(.bottom, 24)
            }
            .refreshable { await model.refresh() }
            .scrollIndicators(.hidden)
            .background(AmbientBackground().ignoresSafeArea())
            .navigationBarTitleDisplayMode(.inline)
            .sheet(isPresented: $showEditor) { HomeEditorSheet() }
            .toolbar {
                ToolbarItem(placement: .topBarLeading) {
                    Button { showEditor = true } label: { Label("Düzenle", systemImage: "slider.horizontal.3").labelStyle(.titleAndIcon).font(.subheadline).foregroundStyle(.white.opacity(0.7)) }
                        .accessibilityLabel("Ana ekranı düzenle")
                }
                ToolbarItem(placement: .topBarTrailing) {
                    Button { showSettings = true } label: { Image(systemName: "gearshape.fill").foregroundStyle(.white.opacity(0.7)) }
                        .accessibilityLabel("Köprü ayarları")
                }
            }
        }
    }
}

struct WarningCard: View {
    @Environment(AppModel.self) private var model
    let message: String
    var retry: Bool

    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "wifi.exclamationmark").font(.title3).foregroundStyle(.orange)
            Text(message).font(.footnote).foregroundStyle(.white.opacity(0.9)).frame(maxWidth: .infinity, alignment: .leading)
            if retry { Button("Tekrar dene") { Task { await model.refresh() } }.font(.footnote.weight(.semibold)).buttonStyle(.bordered).tint(.orange) }
        }
        .padding(14)
        .background(Color.orange.opacity(0.14), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
    }
}

/// Demo modu acikken ustte durur: gercek cihaz yok, her sey ornek.
struct DemoBanner: View {
    @Environment(AppModel.self) private var model
    @State private var off = false
    var body: some View {
        HStack(spacing: 12) {
            Image(systemName: "play.rectangle.fill").foregroundStyle(Color.lampOn)
            VStack(alignment: .leading, spacing: 2) {
                Text("Demo modu").font(.footnote.weight(.semibold))
                Text("Örnek cihazlar. Her şeyi dene: cihaz ekle, servo, programlar, ayarlar.").font(.caption).foregroundStyle(.secondary)
            }
            Spacer()
            Button("Kapat") { DemoMode.disable(); model.reloadConfig(); off.toggle() }
                .font(.footnote.weight(.semibold)).buttonStyle(.bordered).tint(Color.lampOn)
        }
        .padding(12)
        .background(Color.lampOn.opacity(0.12), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
    }
}

/// Acik isik varken ustte hafif sicak bir isima: odanin havasini yansitir.
struct AmbientBackground: View {
    @Environment(AppModel.self) private var model

    var body: some View {
        let glow = (model.state?.onCount ?? 0) > 0
        ZStack {
            Color.black
            RadialGradient(colors: [Color.lampOn.opacity(glow ? 0.20 : 0), .clear], center: .top, startRadius: 0, endRadius: 460)
        }
        .animation(.easeInOut(duration: 0.6), value: glow)
    }
}

struct PressStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.95 : 1)
            .animation(.spring(response: 0.25, dampingFraction: 0.7), value: configuration.isPressed)
    }
}

/// Panel (CYD) ile ayni renkler: acikken sari, kapaliyken koyu gri. Acikken hafif parlar.
struct LightTile: View {
    @Environment(AppModel.self) private var model
    let light: LightID
    var wide = false
    @State private var taps = 0
    @State private var showServo = false

    private var isServo: Bool { _ = model.roles; return light.role == .servo }
    /// Servo hedefe giderken: sari yanar ve "Aciliyor / Kapaniyor / Calisiyor" yazar; varinca "Acik · 90°".
    private var busyLabel: String? {
        guard isServo else { return nil }
        if let m = model.servoMoves[light.rawValue], m.until > Date() { return "\(m.label) \(m.angle())°" }       // canli aci (tahmin)
        if let s = model.state?.servo[light.rawValue], s.moving { return (value == 1 ? "Açılıyor…" : "Kapanıyor…") + (s.cur >= 0 ? " \(s.cur)°" : "") }
        return nil
    }
    private var value: Int { model.state?.value(light) ?? -1 }
    /// Kart yanik: tek isik acikken o isik; "Tum isiklar" icin yalnizca HEPSI aciksa (kismi durumda yanmaz, sadece 1/3 yazar).
    private var on: Bool { _ = model.removed; return busyLabel != nil || (light == .all ? (model.state?.allOn ?? false) : value == 1) }
    private var partial: Bool { _ = model.removed; return light == .all && (model.state?.partial ?? false) }
    private var loading: Bool { model.state == nil && model.error == nil }

    private var status: String {
        _ = model.removed                                  // cihaz kaldirilinca/geri eklenince sayim (1/2, 1/3) tazelensin
        guard let s = model.state else { return "—" }
        if let busy = busyLabel { return busy }
        if light == .all { return s.onCount == 0 ? "Kapalı" : "\(s.onCount)/\(LightID.lights.count)" + (s.allOn ? " açık" : "") }
        if isServo, let a = s.servo[light.rawValue], a.angle >= 0, value != -1 {
            return (on ? "Açık" : "Kapalı") + " · \(a.angle)°" + (a.free ? " · serbest" : "")
        }
        return value == -1 ? "—" : (on ? "Açık" : "Kapalı")
    }

    /// Anahtar: kapali solda, hepsi acik sagda, kismi (bir kismi acik) ortada ve sari izle
    private var switchView: some View {
        ZStack {
            Capsule().fill(on ? Color.black.opacity(0.22) : (partial ? Color.lampOn.opacity(0.9) : Color.white.opacity(0.14)))
                .frame(width: 46, height: 28)
            Circle().fill(on || partial ? Color.white : Color.gray.opacity(0.8)).frame(width: 22, height: 22)
                .offset(x: on ? 9 : (partial ? 0 : -9))
        }
    }

    var body: some View {
        // Servo hareket ederken karo saniyede ~4 kez yenilenir (canli aci); aksi halde durur.
        TimelineView(.animation(minimumInterval: 0.25, paused: busyLabel == nil)) { _ in tile }
            .overlay(alignment: .topTrailing) {
                if isServo {                                    // keşfedilebilir açı ayarı (uzun basmaya gerek yok)
                    Button { showServo = true } label: {
                        Image(systemName: "slider.horizontal.3").font(.system(size: 14, weight: .semibold))
                            .foregroundStyle(on ? Color(hex: 0x14120C) : .white).frame(width: 30, height: 30)
                            .background(.white.opacity(on ? 0.25 : 0.12), in: Circle())
                    }
                    .buttonStyle(.plain).padding(.top, 14).padding(.trailing, 78).accessibilityLabel("\(light.title) açı ayarı")
                }
            }
    }

    @ViewBuilder private var tile: some View {
        let _ = (model.names, model.icons)
        Button {
            taps += 1
            Task { await model.toggle(light) }
        } label: {
            Group {
                if wide {
                    HStack(spacing: 14) {
                        Image(systemName: light.symbol)
                            .font(.system(size: 28, weight: .semibold))
                            .symbolEffect(.bounce, value: on)
                            .frame(width: 36)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(light.title).font(.system(size: 19, weight: .semibold)).lineLimit(1)
                            Text(status).font(.system(size: 14, weight: partial ? .semibold : .medium))
                                .foregroundStyle(partial ? Color.lampOn : (on ? Color(hex: 0x14120C) : .white))
                                .opacity(partial ? 1 : (on ? 0.7 : 0.55))
                        }
                        Spacer()
                        switchView
                    }
                } else {
                    VStack(alignment: .leading, spacing: 0) {
                        HStack {
                            Image(systemName: light.symbol)
                                .font(.system(size: 26, weight: .semibold))
                                .symbolEffect(.bounce, value: on)
                                .symbolEffect(.pulse, isActive: busyLabel != nil)
                            Spacer()
                            switchView
                        }
                        Spacer(minLength: 18)
                        Text(light.title).font(.system(size: 17, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.8)
                        Text(status).font(.system(size: 14, weight: partial ? .semibold : .medium))
                            .foregroundStyle(partial ? Color.lampOn : (on ? Color(hex: 0x14120C) : .white))
                            .opacity(partial ? 1 : (on ? 0.7 : 0.55))
                    }
                }
            }
            .foregroundStyle(on ? Color(hex: 0x14120C) : .white)
            .padding(16)
            .frame(maxWidth: .infinity, minHeight: wide ? 84 : 150, alignment: wide ? .leading : .topLeading)
            .background(
                RoundedRectangle(cornerRadius: 26, style: .continuous)
                    .fill(on ? LinearGradient(colors: [Color(hex: 0xFFD35A), Color(hex: 0xF5B92E)], startPoint: .top, endPoint: .bottom)
                             : LinearGradient(colors: [Color(hex: 0x303033), Color(hex: 0x232326)], startPoint: .top, endPoint: .bottom))
            )
            .shadow(color: on ? Color.lampOn.opacity(0.35) : .clear, radius: 18, y: 6)
            .redacted(reason: loading ? .placeholder : [])
            .opacity(model.state == nil ? 0.55 : 1)
        }
        .buttonStyle(PressStyle())
        .disabled(model.state == nil)
        .sensoryFeedback(.impact(flexibility: .soft), trigger: taps)
        .contextMenu {
            if isServo {
                Button { showServo = true } label: { Label("Açıyı ayarla", systemImage: "slider.horizontal.3") }
                Button { Task { await model.servoFree(light) } } label: { Label("Serbest bırak", systemImage: "hand.raised.fill") }
            }
        }
        .sheet(isPresented: $showServo) { ServoSheet(light: light) }
        .animation(.spring(response: 0.35, dampingFraction: 0.75), value: on)
        .animation(.spring(response: 0.35, dampingFraction: 0.75), value: partial)
        .accessibilityLabel("\(light.title), \(status)")
    }
}
