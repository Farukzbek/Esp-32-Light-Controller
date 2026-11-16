import SwiftUI

/// Rehberli cihaz ekleme: hazirlik -> arama -> bulunan cihaz -> rol -> deneme -> bitti.
struct PairingFlow: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model

    enum Step { case intro, searching, pick, role, test, done }
    @State private var step: Step = .intro
    @State private var status: BridgeClient.PairStatus?
    @State private var selected: BridgeClient.PairCandidate?
    @State private var role: LightID?
    @State private var error: String?
    @State private var busy = false
    @State private var poll: Task<Void, Never>?
    @State private var editing: LightID?

    private var stepIndex: Int { [Step.intro, .searching, .pick, .role, .test, .done].firstIndex(of: step) ?? 0 }

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ProgressView(value: Double(stepIndex), total: 5).tint(Color.lampOn).padding(.horizontal).padding(.top, 8)
                ScrollView {
                    VStack(alignment: .leading, spacing: 18) {
                        switch step {
                        case .intro: intro
                        case .searching: searching
                        case .pick: pick
                        case .role: roles
                        case .test: test
                        case .done: done
                        }
                        if let error {
                            Label(error, systemImage: "exclamationmark.triangle.fill").font(.footnote).foregroundStyle(.orange)
                                .padding(12).frame(maxWidth: .infinity, alignment: .leading)
                                .background(Color.orange.opacity(0.14), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                        }
                    }
                    .padding()
                }
            }
            .background(Color.black.ignoresSafeArea())
            .navigationTitle("Cihaz ekle")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button(step == .done ? "Kapat" : "Vazgeç") { close() } } }
        }
        .interactiveDismissDisabled(step == .searching || step == .role)
        .onDisappear { poll?.cancel() }
        .sheet(item: $editing) { DeviceEditor(light: $0) }
    }

    // MARK: adimlar

    private var intro: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("antenna.radiowaves.left.and.right", "Yeni cihazı hazırla")
            numbered(1, "ESP32'de Nowlight yazılımı olmalı", "Boş bir ESP32'ye önce yazılımı yüklemek gerekir. Bilgisayardaki ya da Android'deki Chrome ile GitHub sayfasındaki rehberi izleyebilirsin.")
            numbered(2, "Cihaza güç ver", "USB adaptörüne ya da powerbank'e tak. Cihaz hiçbir köprüye bağlı değilse kendiliğinden eşleşme modunda bekler.")
            numbered(3, "Telefonu ev WiFi'sine bağla", "Köprü ile aynı ağda olman gerekiyor.")
            Link(destination: AppInfo.docsURL) { Label("Yazılım yükleme rehberi", systemImage: "book.fill") }
            Button { Task { await start() } } label: {
                HStack { if busy { ProgressView().tint(.black) }; Text("Cihaz ara") }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large).disabled(busy)
        }
    }

    private var searching: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("dot.radiowaves.left.and.right", "Cihaz aranıyor…")
            Text("Köprü eşleşme modunda, yeni cihazları dinliyor. Cihaza yeni güç verdiysen birkaç saniye içinde burada görünür.").foregroundStyle(.secondary)
            HStack(spacing: 10) {
                ProgressView()
                Text("\(status?.secondsLeft ?? 0) sn kaldı").monospacedDigit().foregroundStyle(.secondary)
            }
            if let s = status, !s.found.isEmpty {
                ForEach(s.found) { c in candidateRow(c) }
            }
            Button("Aramayı durdur", role: .cancel) { Task { await stop(); step = .intro } }.buttonStyle(.bordered)
        }
    }

    private var pick: some View {
        VStack(alignment: .leading, spacing: 14) {
            header("checkmark.seal.fill", "Cihaz bulundu")
            ForEach(status?.found ?? []) { c in candidateRow(c) }
            Text("Birden fazla cihaz görünüyorsa, istediğini seçmeden önce diğerlerini fişten çekip emin olabilirsin.").font(.footnote).foregroundStyle(.secondary)
        }
    }

    private func candidateRow(_ c: BridgeClient.PairCandidate) -> some View {
        Button { selected = c; error = nil; poll?.cancel(); step = .role } label: {
            HStack(spacing: 12) {
                Image(systemName: "cpu.fill").foregroundStyle(.black).frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
                VStack(alignment: .leading, spacing: 2) {
                    Text(c.name).font(.body.weight(.semibold))
                    Text("\(c.mac) · sinyal \(c.rssi) dBm").font(.caption).foregroundStyle(.secondary)
                }
                Spacer()
                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
            }
            .padding(12).background(Color.white.opacity(0.08), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
        }.buttonStyle(.plain)
    }

    private var roles: some View {
        VStack(alignment: .leading, spacing: 14) {
            header("slider.horizontal.3", "Bu cihaz ne olacak?")
            Text("Rolü seç. Cihaz uygulamada, widget'larda ve otomasyonlarda bu ışığın yerini alır.").foregroundStyle(.secondary)
            ForEach(LightID.allDevices, id: \.self) { light in
                let removed = model.removed.contains(light.rawValue)
                let occupied = !removed && linked(light)
                Button { role = light } label: {
                    HStack(spacing: 12) {
                        Image(systemName: light.symbol).foregroundStyle(.black).frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
                        VStack(alignment: .leading, spacing: 2) {
                            Text(light.title).font(.body.weight(.semibold))
                            Text(removed ? "Kaldırılmış, geri eklenecek" : (occupied ? "Şu an bağlı bir cihaz var, yenisi onun yerine geçer" : "Boş"))
                                .font(.caption).foregroundStyle(occupied ? Color.orange : .secondary)
                        }
                        Spacer()
                        Image(systemName: role == light ? "checkmark.circle.fill" : "circle").foregroundStyle(role == light ? Color.lampOn : .secondary).font(.title3)
                    }
                    .padding(12).background(Color.white.opacity(role == light ? 0.14 : 0.08), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                }.buttonStyle(.plain)
            }
            Button { Task { await accept() } } label: {
                HStack { if busy { ProgressView().tint(.black) }; Text("Bağla") }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large).disabled(role == nil || busy)
        }
    }

    private var test: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("play.circle.fill", "Deneyelim")
            Text("Cihaz bağlandı. Işığı açıp kapatarak çalıştığını kontrol et.").foregroundStyle(.secondary)
            HStack(spacing: 12) {
                Button { Task { await toggleTest(true) } } label: { Label("Aç", systemImage: "lightbulb.fill").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
                Button { Task { await toggleTest(false) } } label: { Label("Kapat", systemImage: "lightbulb.slash").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
            }.tint(Color.lampOn).controlSize(.large)
            Text("Işık açılıp kapandı mı?").font(.subheadline.weight(.semibold)).padding(.top, 6)
            Button { step = .done } label: { Label("Evet, çalışıyor", systemImage: "checkmark").frame(maxWidth: .infinity) }
                .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large)
            Text("Çalışmıyorsa: cihazın fişini çekip tak, röle ve kablo bağlantısını kontrol et. Rol seçimi sonradan Cihaz Ekle ekranından değişmez; cihazı kaldırıp yeniden eklemen gerekir.")
                .font(.footnote).foregroundStyle(.secondary)
        }
    }

    private var done: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("party.popper.fill", "Hazır")
            if let r = role { Label("\(r.title) bağlandı", systemImage: r.symbol).font(.headline) }
            Text("İstersen adını ve simgesini değiştirebilirsin.").foregroundStyle(.secondary)
            if let r = role {
                Button { editing = r } label: { Label("Ad ve simgeyi düzenle", systemImage: "pencil") }
            }
            Button { close() } label: { Text("Bitti").frame(maxWidth: .infinity) }
                .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large)
        }
    }

    // MARK: parcalar

    private func header(_ symbol: String, _ title: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).font(.system(size: 24, weight: .semibold)).foregroundStyle(.black)
                .frame(width: 52, height: 52).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            Text(title).font(.title2.weight(.bold))
        }
    }

    private func numbered(_ n: Int, _ title: String, _ text: String) -> some View {
        HStack(alignment: .top, spacing: 12) {
            Text("\(n)").font(.subheadline.weight(.bold)).foregroundStyle(.black).frame(width: 26, height: 26).background(Color.lampOn, in: Circle())
            VStack(alignment: .leading, spacing: 3) {
                Text(title).font(.body.weight(.semibold))
                Text(text).font(.subheadline).foregroundStyle(.secondary)
            }
        }
    }

    private func linked(_ light: LightID) -> Bool {
        guard let s = model.state else { return false }
        switch light { case .masa, .led: return s.links.masa; case .yatak: return s.links.yatak; case .all: return false }
    }

    // MARK: ag

    private func start() async {
        error = nil; busy = true; defer { busy = false }
        do {
            let c = try BridgeClient.fromConfig()
            status = try await c.pairStart()
            step = .searching
            poll?.cancel()
            poll = Task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(1))
                    guard let st = try? await c.pairStatus() else { continue }
                    await MainActor.run {
                        status = st
                        if !st.found.isEmpty && step == .searching { step = .pick }
                        if !st.open && st.found.isEmpty && step == .searching { error = "Süre doldu, cihaz bulunamadı. Cihaza güç verdiğinden emin ol ve tekrar ara."; step = .intro }
                    }
                }
            }
        } catch BridgeError.badResponse {
            self.error = "Köprü yazılımı henüz cihaz eşleştirmeyi desteklemiyor. Köprüyü güncellemen gerekiyor."
        } catch { self.error = (error as? LocalizedError)?.errorDescription ?? "Eşleşme başlatılamadı." }
    }

    private func stop() async {
        poll?.cancel()
        if let c = try? BridgeClient.fromConfig() { await c.pairStop() }
    }

    private func accept() async {
        guard let sel = selected, let role else { return }
        error = nil; busy = true; defer { busy = false }
        do {
            try await BridgeClient.fromConfig().pairAccept(mac: sel.mac, role: role)
            if model.removed.contains(role.rawValue) { model.restore(role) }
            await model.refresh()
            step = .test
        } catch { self.error = (error as? LocalizedError)?.errorDescription ?? "Cihaz bağlanamadı. Tekrar dene." }
    }

    private func toggleTest(_ on: Bool) async {
        guard let role else { return }
        do { try await BridgeClient.fromConfig().set(role, on: on); error = nil }
        catch { self.error = (error as? LocalizedError)?.errorDescription ?? "Komut gönderilemedi." }
    }

    private func close() {
        Task { await stop() }
        dismiss()
    }
}
