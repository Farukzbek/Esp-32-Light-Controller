import SwiftUI

/// Sahneler: liste ve duzenleyici. Her sahnede her cihaz icin "Dokunma / Aç / Kapat / Açıya git" secilir.
struct ScenesScreen: View {
    @Environment(AppModel.self) private var model
    @State private var editing: SceneDef?
    @State private var isNew = false

    var body: some View {
        Form {
            Section {
                if model.scenes.isEmpty {
                    Text("Henüz sahne yok. Bir sahne, birkaç cihazı tek dokunuşla ayarlar: \"Akşam\" salonu açıp yatak odasını kapatır, perdeyi kapatır.").font(.subheadline).foregroundStyle(.secondary)
                    Button { addTemplate(evening: true) } label: { Label("Hazır: Akşam", systemImage: "moon.stars.fill") }
                    Button { addTemplate(evening: false) } label: { Label("Hazır: Hepsini kapat", systemImage: "power") }
                }
                ForEach(model.scenes) { s in
                    Button { isNew = false; editing = s } label: {
                        HStack(spacing: 12) {
                            Image(systemName: s.symbol).foregroundStyle(.black).frame(width: 36, height: 36)
                                .background(Color.lampOn, in: RoundedRectangle(cornerRadius: 10, style: .continuous))
                            VStack(alignment: .leading, spacing: 2) {
                                Text(s.name).font(.body.weight(.semibold))
                                Text(s.summary).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                            }
                            Spacer()
                            Button { Task { await model.runScene(s) } } label: { Image(systemName: "play.circle.fill").font(.title2).foregroundStyle(Color.lampOn) }
                                .buttonStyle(.plain).accessibilityLabel("\(s.name) sahnesini çalıştır")
                        }
                    }.buttonStyle(.plain)
                }
                .onDelete { model.deleteScenes(at: $0) }
                Button { isNew = true; editing = SceneDef(steps: LightID.controllable.map { SceneStep(device: $0.rawValue) }) } label: { Label("Sahne ekle", systemImage: "plus.circle.fill") }
            } header: { Text("Sahneler") } footer: {
                Text("Sahneyi buradan, ana ekrandaki düğmeden, Siri'den (\"Nowlight Akşam sahnesi\") ya da Denetim Merkezi / Action Button'dan çalıştırabilirsin. Telefon ev ağındayken çalışır.")
            }
            if let msg = model.sceneMessage { Section { Text(msg).font(.footnote).foregroundStyle(msg.hasPrefix("✓") ? .green : .orange) } }
        }
        .navigationTitle("Sahneler")
        .navigationBarTitleDisplayMode(.inline)
        .toolbar { EditButton() }
        .sheet(item: $editing) { s in SceneEditor(scene: s, isNew: isNew) { model.upsertScene($0) } }
    }

    private func addTemplate(evening: Bool) {
        let devs = LightID.controllable
        var s = SceneDef(name: evening ? "Akşam" : "Hepsini kapat", symbol: evening ? "moon.stars.fill" : "power")
        s.steps = devs.enumerated().map { i, d in
            var st = SceneStep(device: d.rawValue)
            if d.role == .servo { st.mode = evening ? .angle : .off; st.angle = 0 }
            else { st.mode = evening ? (i == 0 ? .on : .off) : .off }
            return st
        }
        model.upsertScene(s)
    }
}

private struct SceneEditor: View {
    @Environment(\.dismiss) private var dismiss
    @State var scene: SceneDef
    let isNew: Bool
    var onSave: (SceneDef) -> Void

    private static let symbols = ["sparkles", "moon.stars.fill", "sun.max.fill", "bed.double.fill", "sofa.fill", "tv.fill", "fork.knife", "book.fill", "power", "house.fill", "figure.walk", "party.popper.fill"]

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("Sahnenin adı", text: $scene.name)
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 8) {
                            ForEach(Self.symbols, id: \.self) { sym in
                                Button { scene.symbol = sym } label: {
                                    Image(systemName: sym).font(.system(size: 18, weight: .semibold))
                                        .foregroundStyle(scene.symbol == sym ? .black : .white).frame(width: 44, height: 44)
                                        .background(scene.symbol == sym ? Color.lampOn : Color.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                                }.buttonStyle(.plain)
                            }
                        }
                    }
                }
                Section {
                    ForEach(LightID.controllable, id: \.self) { d in
                        let i = index(of: d)
                        VStack(alignment: .leading, spacing: 6) {
                            Label(d.title, systemImage: d.symbol)
                            Picker("", selection: Binding(get: { scene.steps[i].mode }, set: { scene.steps[i].mode = $0 })) {
                                ForEach(d.role == .servo ? SceneStep.Mode.allCases : [.skip, .on, .off], id: \.self) { Text($0.title).tag($0) }
                            }.pickerStyle(.segmented).labelsHidden()
                            if scene.steps[i].mode == .angle {
                                HStack { Text("Açı"); Spacer(); Text("\(scene.steps[i].angle)°").foregroundStyle(.secondary) }
                                Slider(value: Binding(get: { Double(scene.steps[i].angle) }, set: { scene.steps[i].angle = Int($0) }), in: 0...180, step: 5).tint(Color.lampOn)
                            }
                        }.padding(.vertical, 2)
                    }
                } header: { Text("Cihazlar") } footer: { Text("\"Dokunma\": sahne o cihazın durumunu değiştirmez.") }
            }
            .navigationTitle(isNew ? "Yeni sahne" : "Sahne")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Vazgeç") { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Kaydet") { onSave(scene); dismiss() }.disabled(scene.name.trimmingCharacters(in: .whitespaces).isEmpty)
                }
            }
            .onAppear { syncSteps() }
        }
        .preferredColorScheme(.dark)
    }

    /// Her gorunen cihaz icin bir adim olsun (sonradan eklenen cihazlar da secilebilsin).
    private func syncSteps() {
        for d in LightID.controllable where !scene.steps.contains(where: { $0.device == d.rawValue }) { scene.steps.append(SceneStep(device: d.rawValue)) }
    }
    private func index(of d: LightID) -> Int {
        if let i = scene.steps.firstIndex(where: { $0.device == d.rawValue }) { return i }
        scene.steps.append(SceneStep(device: d.rawValue)); return scene.steps.count - 1
    }
}

/// Ana ekranda sahne dugmeleri (yatay serit).
struct SceneChips: View {
    @Environment(AppModel.self) private var model
    var body: some View {
        if !model.scenes.isEmpty {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(model.scenes) { s in
                        Button { Task { await model.runScene(s) } } label: {
                            Label(s.name, systemImage: s.symbol).font(.subheadline.weight(.semibold)).padding(.horizontal, 14).padding(.vertical, 10)
                                .background(Color.white.opacity(0.1), in: Capsule())
                        }
                        .buttonStyle(PressStyle()).foregroundStyle(.white)
                        .accessibilityLabel("\(s.name) sahnesini çalıştır")
                    }
                }
                .padding(.horizontal, 2)
            }
            .sensoryFeedback(.impact(flexibility: .soft), trigger: model.sceneRuns)
        }
    }
}
