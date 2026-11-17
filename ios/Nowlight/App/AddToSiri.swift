import SwiftUI
import AppIntents
import Intents
import IntentsUI

/// "Siri'ye ekle" sayfasi: secilen isik icin "<ad> ac" / "<ad> kapat" gibi kendi cumlenle calisan bir Siri komutu olusturur.
/// Siri bu cumleyi uygulama adi soylemeden, birebir taniyor.
struct AddToSiriSheet: UIViewControllerRepresentable {
    let light: LightID
    let on: Bool
    let phrase: String
    var onFinish: (String) -> Void

    func makeUIViewController(context: Context) -> INUIAddVoiceShortcutViewController {
        let intent = SetLightIntent()
        intent.light = light.rawValue
        intent.on = NSNumber(value: on)
        intent.suggestedInvocationPhrase = phrase
        let vc = INUIAddVoiceShortcutViewController(shortcut: INShortcut(intent: intent)!)
        vc.delegate = context.coordinator
        return vc
    }
    func updateUIViewController(_ vc: INUIAddVoiceShortcutViewController, context: Context) {}
    func makeCoordinator() -> Coordinator { Coordinator(onFinish) }

    final class Coordinator: NSObject, INUIAddVoiceShortcutViewControllerDelegate {
        let onFinish: (String) -> Void
        init(_ f: @escaping (String) -> Void) { onFinish = f }
        func addVoiceShortcutViewController(_ c: INUIAddVoiceShortcutViewController, didFinishWith voiceShortcut: INVoiceShortcut?, error: Error?) {
            if let v = voiceShortcut { onFinish("✓ Eklendi: \"\(v.invocationPhrase)\"") }
            else if let error { onFinish(error.localizedDescription) }
            else { onFinish("") }
        }
        func addVoiceShortcutViewControllerDidCancel(_ c: INUIAddVoiceShortcutViewController) { onFinish("") }
    }
}

// MARK: - Toplu ekleme

/// "Siri'ye ekle" kartı: Işıklar ekranının en altında durur.
struct SiriPromoCard: View {
    @State private var show = false
    var body: some View {
        Button { show = true } label: {
            HStack(spacing: 12) {
                Image(systemName: "mic.badge.plus").font(.title3).foregroundStyle(Color.lampOn)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Siri'ye ekle").font(.subheadline.weight(.semibold)).foregroundStyle(.white)
                    Text("\"Led kapat\" gibi komutlarla ışıkları sesinle yönet. Telefon kilitliyken de çalışır.")
                        .font(.caption).foregroundStyle(.secondary).multilineTextAlignment(.leading)
                }
                Spacer(minLength: 8)
                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.secondary)
            }
            .padding(14)
            .background(Color.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 18, style: .continuous))
        }
        .buttonStyle(.plain)
        .sheet(isPresented: $show) { SiriBatchSheet() }
    }
}

/// Işıkları seçtirir, seçilenlerin "aç" ve "kapat" komutlarını art arda Siri'ye ekletir.
struct SiriBatchSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model

    struct Item: Identifiable, Equatable {
        let light: LightID
        let on: Bool
        var id: String { light.rawValue + (on ? "1" : "0") }
        var phrase: String { LightID.spokenPhrase(light, on: on) }
    }

    @State private var selected: Set<String> = []
    @State private var existing: Set<String> = []
    @State private var queue: [Item] = []
    @State private var index = 0
    @State private var current: Item?
    @State private var added = 0
    @State private var running = false
    @State private var message: String?

    private var choices: [LightID] {
        _ = (model.names, model.removed)
        return LightID.controllable + (LightID.lights.count >= 2 ? [.all] : [])
    }
    private func has(_ l: LightID) -> Bool { existing.contains(Item(light: l, on: true).id) && existing.contains(Item(light: l, on: false).id) }

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    ForEach(choices, id: \.self) { l in
                        Button { toggle(l) } label: {
                            HStack(spacing: 12) {
                                Image(systemName: l.symbol).frame(width: 28).foregroundStyle(Color.lampOn)
                                VStack(alignment: .leading, spacing: 2) {
                                    Text(l.title).foregroundStyle(.white)
                                    Text("\"\(LightID.spokenPhrase(l, on: true))\" · \"\(LightID.spokenPhrase(l, on: false))\"").font(.caption).foregroundStyle(.secondary)
                                }
                                Spacer()
                                if has(l) { Text("Ekli").font(.caption).foregroundStyle(.green) }
                                Image(systemName: selected.contains(l.rawValue) ? "checkmark.circle.fill" : "circle")
                                    .foregroundStyle(selected.contains(l.rawValue) ? Color.lampOn : .secondary)
                            }
                        }.disabled(running)
                    }
                } header: { Text("Işıklar") } footer: {
                    Text("Seçtiğin her ışık için \"aç\" ve \"kapat\" komutu eklenir. Siri her komutu bir kez sesli söylemeni ister (Apple'ın zorunlu adımı). Sayfalar art arda kendiliğinden açılır, sen yalnızca \"Siri'ye ekle\"ye basıp cümleyi söylersin.")
                }
                if let message { Section { Text(message).font(.footnote).foregroundStyle(message.hasPrefix("✓") ? .green : .orange) } }
                Section {
                    Button { start() } label: { Label("Seçilenleri ekle (\(pendingCount))", systemImage: "mic.badge.plus") }
                        .disabled(running || pendingCount == 0)
                }
            }
            .navigationTitle("Siri'ye ekle")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .cancellationAction) { Button("Kapat") { dismiss() } } }
            .sheet(item: $current) { item in
                AddToSiriSheet(light: item.light, on: item.on, phrase: item.phrase) { msg in
                    if msg.hasPrefix("✓") { added += 1; existing.insert(item.id) }
                    current = nil
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.6) { index += 1; next() }
                }.ignoresSafeArea()
            }
            .task { await loadExisting() }
        }
        .preferredColorScheme(.dark)
    }

    private var pendingCount: Int {
        selected.reduce(0) { n, raw in
            guard let l = LightID(rawValue: raw) else { return n }
            return n + [true, false].filter { !existing.contains(Item(light: l, on: $0).id) }.count
        }
    }

    private func toggle(_ l: LightID) {
        if selected.contains(l.rawValue) { selected.remove(l.rawValue) } else { selected.insert(l.rawValue) }
    }

    private func start() {
        queue = choices.filter { selected.contains($0.rawValue) }
            .flatMap { l in [true, false].map { Item(light: l, on: $0) } }
            .filter { !existing.contains($0.id) }
        guard !queue.isEmpty else { return }
        index = 0; added = 0; running = true; message = nil
        next()
    }

    private func next() {
        if index < queue.count { current = queue[index]; return }
        running = false
        message = added > 0 ? "✓ \(added) komut eklendi. Telefon kilitliyken de çalışır." : "Hiç komut eklenmedi."
        selected.removeAll()
    }

    private func loadExisting() async {
        let ids: [String] = await withCheckedContinuation { c in
            INVoiceShortcutCenter.shared.getAllVoiceShortcuts { list, _ in
                c.resume(returning: (list ?? []).compactMap { v in
                    guard let i = v.shortcut.intent as? SetLightIntent, let l = i.light, let on = i.on else { return nil }
                    return l + (on.boolValue ? "1" : "0")
                })
            }
        }
        existing = Set(ids)
    }
}

extension LightID {
    /// Siri'de soylenecek cumle: "led kapat", "tüm ışıkları aç"
    static func spokenPhrase(_ l: LightID, on: Bool) -> String {
        let name = l == .all ? "tüm ışıkları" : l.shortTitle.lowercased(with: Locale(identifier: "tr_TR"))
        return "\(name) \(on ? "aç" : "kapat")"
    }
}
