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
            WidgetsScreen()
                .tabItem { Label("Widgetlar", systemImage: "square.grid.2x2.fill") }.tag(1)
            AddDeviceScreen(showSettings: $showSettings)
                .tabItem { Label("Cihaz Ekle", systemImage: "plus.circle.fill") }.tag(2)
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
            if !UserDefaults.standard.bool(forKey: "welcomeSeen") { showWelcome = true }
            else if !BridgeConfig.isConfigured { showSettings = true }
        }
    }
}

/// Ana ekran: yalnizca isik dugmeleri. Baglanti sorunu olursa ustte kisa bir uyari cikar.
struct LightsScreen: View {
    @Environment(AppModel.self) private var model
    @Binding var showSettings: Bool

    private let columns = [GridItem(.flexible(), spacing: 14), GridItem(.flexible(), spacing: 14)]

    private var tiles: [LightID] {
        let d = LightID.devices
        _ = model.removed                                  // kaldirma/geri ekleme gorunumu tazeler
        return d.count >= 2 ? d + [.all] : d
    }

    private var warning: String? {
        if let e = model.error { return e }
        guard let s = model.state else { return nil }
        let masa = !LightID.devices.contains(.masa) || s.links.masa
        let yatak = !LightID.devices.contains(.yatak) || s.links.yatak
        switch (masa, yatak) {
        case (true, true): return nil
        case (false, false): return "Masa ve yatak düğümleri ulaşılamıyor"
        case (false, true): return "Masa düğümü ulaşılamıyor"
        case (true, false): return "Yatak düğümü ulaşılamıyor"
        }
    }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 14) {
                    if let w = warning { WarningCard(message: w, retry: model.error != nil) }
                    if tiles.isEmpty {
                        ContentUnavailableView("Cihaz yok", systemImage: "lightbulb.slash",
                                               description: Text("Cihaz Ekle sekmesinden kaldırdığın cihazları geri ekleyebilirsin."))
                            .padding(.top, 60)
                    } else {
                        LazyVGrid(columns: columns, spacing: 14) {
                            ForEach(tiles, id: \.self) { LightTile(light: $0) }
                        }
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
            .toolbar {
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
    @State private var taps = 0

    private var value: Int { model.state?.value(light) ?? -1 }
    /// Kart yanik: tek isik acikken o isik; "Tum isiklar" icin yalnizca HEPSI aciksa (kismi durumda yanmaz, sadece 1/3 yazar).
    private var on: Bool { _ = model.removed; return light == .all ? (model.state?.allOn ?? false) : value == 1 }
    private var partial: Bool { _ = model.removed; return light == .all && (model.state?.partial ?? false) }
    private var loading: Bool { model.state == nil && model.error == nil }

    private var status: String {
        _ = model.removed                                  // cihaz kaldirilinca/geri eklenince sayim (1/2, 1/3) tazelensin
        guard let s = model.state else { return "—" }
        if light == .all { return s.onCount == 0 ? "Kapalı" : "\(s.onCount)/\(LightID.devices.count)" + (s.allOn ? " açık" : "") }
        return value == -1 ? "—" : (on ? "Açık" : "Kapalı")
    }

    var body: some View {
        let _ = (model.names, model.icons)
        Button {
            taps += 1
            Task { await model.toggle(light) }
        } label: {
            VStack(alignment: .leading, spacing: 0) {
                HStack {
                    Image(systemName: light.symbol)
                        .font(.system(size: 26, weight: .semibold))
                        .symbolEffect(.bounce, value: on)
                    Spacer()
                    // Anahtar: kapali solda, hepsi acik sagda, kismi (bir kismi acik) ortada ve sari izle
                    ZStack {
                        Capsule().fill(on ? Color.black.opacity(0.22) : (partial ? Color.lampOn.opacity(0.9) : Color.white.opacity(0.14)))
                            .frame(width: 46, height: 28)
                        Circle().fill(on || partial ? Color.white : Color.gray.opacity(0.8)).frame(width: 22, height: 22)
                            .offset(x: on ? 9 : (partial ? 0 : -9))
                    }
                }
                Spacer(minLength: 18)
                Text(light.title).font(.system(size: 17, weight: .semibold)).lineLimit(1).minimumScaleFactor(0.8)
                Text(status).font(.system(size: 14, weight: partial ? .semibold : .medium))
                    .foregroundStyle(partial ? Color.lampOn : (on ? Color(hex: 0x14120C) : .white))
                    .opacity(partial ? 1 : (on ? 0.7 : 0.55))
            }
            .foregroundStyle(on ? Color(hex: 0x14120C) : .white)
            .padding(16)
            .frame(maxWidth: .infinity, minHeight: 150, alignment: .topLeading)
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
        .animation(.spring(response: 0.35, dampingFraction: 0.75), value: on)
        .animation(.spring(response: 0.35, dampingFraction: 0.75), value: partial)
        .accessibilityLabel("\(light.title), \(status)")
    }
}
