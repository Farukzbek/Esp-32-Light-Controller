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
    @State private var addingNew = false
    @State private var newName = ""
    @State private var newSymbol = "lightbulb.fill"
    @State private var newRole: NodeRole = .light
    @State private var svOff = 0
    @State private var svOn = 90
    @State private var relayPin = 5
    @State private var relayInv = true
    @State private var error: String?
    @State private var busy = false
    @State private var poll: Task<Void, Never>?
    @State private var editing: LightID?
    @State private var searchStart = Date()
    @State private var foundAfter: Int?
    @State private var connectedNow = false        // kopru bu ekrandan baglandi: karta basarili mesaji gostermeye devam et

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
            if connectedNow || !BridgeConfig.isConfigured { BridgeConnectCard { connectedNow = true } }       // connectedNow ONCE okunur: SwiftUI bu durumu izlesin (kisa devre olmasin)
            numbered(1, "ESP32'de Nowlight yazılımı olmalı", "Boş bir ESP32'ye önce yazılımı yüklemek gerekir. Bilgisayardaki ya da Android'deki Chrome ile flash.matestudios.online sayfasını aç, kartını seç ve Yükle'ye bas.")
            numbered(2, "Cihaza güç ver", "USB adaptörüne ya da powerbank'e tak. Cihaz hiçbir köprüye bağlı değilse kendiliğinden eşleşme modunda bekler.")
            numbered(3, "Telefonu ev WiFi'sine bağla", "Köprü ile aynı ağda olman gerekiyor.")
            Link(destination: AppInfo.flashURL) { Label("Yazılım yükleyici (flash.matestudios.online)", systemImage: "arrow.up.right.square") }
            Button { Task { await start() } } label: {
                HStack { if busy { ProgressView().tint(.black) }; Text("Cihaz ara") }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large).disabled(busy || !(connectedNow || BridgeConfig.isConfigured))
            if !(connectedNow || BridgeConfig.isConfigured) { Text("Cihaz aramak için önce köprüyü bağla (yukarıdaki kart).").font(.footnote).foregroundStyle(.secondary) }
        }
    }

    private var searching: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("dot.radiowaves.left.and.right", "Cihaz aranıyor…")
            Text("Köprü eşleşme modunda, yeni cihazları dinliyor. Cihaza yeni güç verdiysen birkaç saniye içinde burada görünür.").foregroundStyle(.secondary)
            HStack(spacing: 10) {
                ProgressView()
                TimelineView(.periodic(from: searchStart, by: 1)) { ctx in
                    Text("\(Int(ctx.date.timeIntervalSince(searchStart))) sn").monospacedDigit().foregroundStyle(.secondary)
                }
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
            if let t = foundAfter { Text("\(t) saniyede bulundu").font(.subheadline).foregroundStyle(.secondary) }
            ForEach(status?.found ?? []) { c in candidateRow(c) }
            Text("Birden fazla cihaz görünüyorsa, istediğini seçmeden önce diğerlerini fişten çekip emin olabilirsin.").font(.footnote).foregroundStyle(.secondary)
        }
    }

    private func candidateRow(_ c: BridgeClient.PairCandidate) -> some View {
        Button { selected = c; relayPin = c.pin ?? 5; relayInv = (c.inv ?? 1) != 0; error = nil; poll?.cancel(); step = .role } label: {
            HStack(spacing: 12) {
                Image(systemName: "cpu.fill").foregroundStyle(.black).frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
                VStack(alignment: .leading, spacing: 2) {
                    Text(c.name).font(.body.weight(.semibold))
                    Text("\(c.chipName) · \(c.mac) · sinyal \(c.rssi) dBm").font(.caption).foregroundStyle(.secondary)
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
            Text("Cihazın görevini seç: ışık, servo (perde, mekanik anahtar...) ya da hareket sensörü.").foregroundStyle(.secondary)

            // Yeni cihaz: rol sec
            ForEach(NodeRole.allCases, id: \.self) { r in
                let selectedRole = addingNew && newRole == r
                Button {
                    addingNew = true; role = nil; newRole = r; newSymbol = r.symbol
                    relayPin = PinGuide.defaultPin(for: r, chip: selected?.chip, reported: selected?.pin)
                } label: {
                    HStack(spacing: 12) {
                        Image(systemName: r.symbol).font(.system(size: 18, weight: .bold)).foregroundStyle(.black).frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
                        VStack(alignment: .leading, spacing: 2) {
                            Text(r == .light ? "Yeni ışık olarak ekle" : r == .servo ? "Servo olarak ekle" : "Hareket sensörü olarak ekle").font(.body.weight(.semibold))
                            Text(LightID.nextCustom() == nil ? "En fazla \(LightID.maxDevices) cihaz eklenebilir"
                                 : r == .light ? "Örneğin salon lambası, mutfak, koridor" : r == .servo ? "Açı kontrollü; perde, kapak, mekanik anahtar" : "Hareket algılar, otomasyonlarda kullanılır").font(.caption).foregroundStyle(.secondary)
                        }
                        Spacer()
                        Image(systemName: selectedRole ? "checkmark.circle.fill" : "circle").foregroundStyle(selectedRole ? Color.lampOn : .secondary).font(.title3)
                    }
                    .padding(12).background(Color.white.opacity(selectedRole ? 0.14 : 0.08), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                }.buttonStyle(.plain).disabled(LightID.nextCustom() == nil)
            }

            if addingNew {
                VStack(alignment: .leading, spacing: 12) {
                    HStack(spacing: 12) {
                        Image(systemName: newSymbol).font(.system(size: 22, weight: .semibold)).foregroundStyle(.black)
                            .frame(width: 48, height: 48).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 13, style: .continuous))
                        TextField(newRole == .light ? "Işığın adı (örn. Salon)" : newRole == .servo ? "Servonun adı (örn. Perde)" : "Sensörün adı (örn. Kapı)", text: $newName).font(.title3.weight(.semibold)).textFieldStyle(.roundedBorder)
                    }
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 8) {
                            ForEach(IconChoices.all, id: \.self) { sym in
                                Button { newSymbol = sym } label: {
                                    Image(systemName: sym).font(.system(size: 18, weight: .semibold))
                                        .foregroundStyle(newSymbol == sym ? .black : .white).frame(width: 44, height: 44)
                                        .background(newSymbol == sym ? Color.lampOn : Color.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                                }.buttonStyle(.plain)
                            }
                        }
                    }
                }
                .padding(12).background(Color.white.opacity(0.06), in: RoundedRectangle(cornerRadius: 14, style: .continuous))

                DisclosureGroup("Pin rehberi (\(PinGuide.chipName(selected?.chip)))") {
                    PinGuideView(chip: selected?.chip, role: newRole).padding(.top, 6)
                }
                .padding(12).background(Color.white.opacity(0.06), in: RoundedRectangle(cornerRadius: 14, style: .continuous))

                DisclosureGroup(newRole == .light ? "Röle ayarı (gerekirse)" : newRole == .servo ? "Servo ayarı (gerekirse)" : "Sensör ayarı (gerekirse)") {
                    VStack(alignment: .leading, spacing: 10) {
                        Stepper(value: $relayPin, in: 0...48) { HStack { Text(newRole == .light ? "Röle pini" : newRole == .servo ? "Servo sinyal pini" : "Sensör pini"); Spacer(); Text("GPIO\(relayPin)").foregroundStyle(.secondary) } }
                        PinHint(chip: selected?.chip, role: newRole, pin: relayPin)
                        if newRole == .light {
                            Toggle("Ters mantık (LOW = açık)", isOn: $relayInv).tint(Color.lampOn)
                            Text("Çoğu 5V röle modülü ters mantıkla çalışır. Işık tam tersi davranırsa kapat. Pini kartına göre değiştir.").font(.footnote).foregroundStyle(.secondary)
                        } else if newRole == .servo {
                            Stepper(value: $svOff, in: 0...180, step: 5) { HStack { Text("Kapalı açısı"); Spacer(); Text("\(svOff)°").foregroundStyle(.secondary) } }
                            Stepper(value: $svOn, in: 0...180, step: 5) { HStack { Text("Açık açısı"); Spacer(); Text("\(svOn)°").foregroundStyle(.secondary) } }
                            Text("Servoyu kendi 5V kaynağından besle (kartın USB'sinden değil) ve GND'leri birleştir. Hız, bırakma süresi ve açı sınırları cihaz ayarlarından değişir.").font(.footnote).foregroundStyle(.secondary)
                        } else {
                            Text("Çoğu PIR modülü 3,3 V sinyal verir ve 5V ile beslenir. Tutma süresi ve hassasiyet cihaz ayarlarından değişir.").font(.footnote).foregroundStyle(.secondary)
                        }
                    }.padding(.top, 6)
                }
                .padding(12).background(Color.white.opacity(0.06), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            }

            // Yerlesik isiklarin (masa, yatak, led) kopruda sabit karti var; yalnizca eklenen isiklarin yerine gecilebilir.
            let replaceable = LightID.allDevices.filter { !$0.isBuiltin }
            if !replaceable.isEmpty {
                Text("Eklediğin bir ışığın yerine geçir").font(.footnote.weight(.semibold)).foregroundStyle(.secondary).padding(.top, 4)
                ForEach(replaceable, id: \.self) { light in
                    let removed = model.removed.contains(light.rawValue)
                    Button { role = light; addingNew = false } label: {
                        HStack(spacing: 12) {
                            Image(systemName: light.symbol).foregroundStyle(.black).frame(width: 40, height: 40).background(Color.lampOn, in: RoundedRectangle(cornerRadius: 11, style: .continuous))
                            VStack(alignment: .leading, spacing: 2) {
                                Text(light.title).font(.body.weight(.semibold))
                                Text(removed ? "Kaldırılmış, geri eklenecek" : "Yeni cihaz bunun yerine geçer").font(.caption).foregroundStyle(.secondary)
                            }
                            Spacer()
                            Image(systemName: role == light ? "checkmark.circle.fill" : "circle").foregroundStyle(role == light ? Color.lampOn : .secondary).font(.title3)
                        }
                        .padding(12).background(Color.white.opacity(role == light ? 0.14 : 0.08), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                    }.buttonStyle(.plain)
                }
            }

            Button { Task { await accept() } } label: {
                HStack { if busy { ProgressView().tint(.black) }; Text("Bağla") }.frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent).tint(Color.lampOn).foregroundStyle(.black).controlSize(.large)
            .disabled((role == nil && !(addingNew && !newName.trimmingCharacters(in: .whitespaces).isEmpty)) || busy)
        }
    }

    private var test: some View {
        VStack(alignment: .leading, spacing: 16) {
            header("play.circle.fill", "Deneyelim")
            if role?.role == .pir {
                Text("Cihaz bağlandı. Sensörün önünde hareket et: aşağıda \"Hareket var\" görünmeli.").foregroundStyle(.secondary)
                let moving = (model.state?.value(role ?? .all) ?? 0) == 1
                Label(moving ? "Hareket var" : "Hareket yok", systemImage: "figure.walk.motion")
                    .font(.title3.weight(.semibold)).foregroundStyle(moving ? Color.lampOn : .secondary)
                    .frame(maxWidth: .infinity).padding(16).background(Color.white.opacity(moving ? 0.14 : 0.07), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                Text("Hareket algılandı mı?").font(.subheadline.weight(.semibold)).padding(.top, 6)
            } else {
                let isServo = role?.role == .servo
                Text(isServo ? "Cihaz bağlandı. Servoyu iki konuma gönderip çalıştığını kontrol et." : "Cihaz bağlandı. Işığı açıp kapatarak çalıştığını kontrol et.").foregroundStyle(.secondary)
                HStack(spacing: 12) {
                    Button { Task { await toggleTest(true) } } label: { Label(isServo ? "Açık konum" : "Aç", systemImage: isServo ? "arrow.up.right.circle.fill" : "lightbulb.fill").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
                    Button { Task { await toggleTest(false) } } label: { Label(isServo ? "Kapalı konum" : "Kapat", systemImage: isServo ? "arrow.down.left.circle" : "lightbulb.slash").frame(maxWidth: .infinity) }.buttonStyle(.bordered)
                }.tint(Color.lampOn).controlSize(.large)
                Text(isServo ? "Servo hareket etti mi?" : "Işık açılıp kapandı mı?").font(.subheadline.weight(.semibold)).padding(.top, 6)
            }
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

    // MARK: ag

    private func start() async {
        error = nil; busy = true; defer { busy = false }
        do {
            let c = try BridgeClient.fromConfig()
            status = try await c.pairStart()
            searchStart = Date(); foundAfter = nil
            step = .searching
            poll?.cancel()
            poll = Task {
                while !Task.isCancelled {
                    try? await Task.sleep(for: .seconds(1))
                    guard let st = try? await c.pairStatus() else { continue }
                    await MainActor.run {
                        status = st
                        if !st.found.isEmpty && step == .searching { foundAfter = Int(Date().timeIntervalSince(searchStart)); step = .pick }
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
        guard let sel = selected else { return }
        error = nil; busy = true; defer { busy = false }
        // Yeni isik: bos yuvanin kimligi; mevcut: o kimlik
        let target: LightID
        if addingNew {
            guard let id = LightID.nextCustom() else { self.error = "En fazla \(LightID.maxDevices) ışık eklenebilir."; return }
            target = id
        } else if let r = role { target = r } else { return }
        do {
            try await BridgeClient.fromConfig().pairAccept(mac: sel.mac, id: target.rawValue, pin: addingNew ? relayPin : nil, inv: addingNew ? (newRole == .light ? relayInv : false) : nil, role: addingNew ? newRole : .light)
            if addingNew {
                model.addDevice(target, name: newName, symbol: newSymbol, pin: relayPin, inv: newRole == .light ? relayInv : false, role: newRole, servoOff: svOff, servoOn: svOn, chip: sel.chip)
                if newRole == .servo { try? await model.setServo(target, ServoSettings(pin: relayPin, inv: false, off: svOff, on: svOn)) }
                if newRole == .pir { try? await model.setPir(target, PirSettings(pin: relayPin)) }
            }
            else if model.removed.contains(target.rawValue) { model.restore(target) }
            role = target
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
