import SwiftUI

/// Ana ekrani duzenle: kartlari surukleyerek sirala, istemedigini gizle, "Tum isiklar" ve Siri kartini ac/kapat.
/// Gizlemek cihazi silmez: widget, Siri ve otomasyonlar etkilenmez (silmek icin Cihaz Ekle sekmesi).
struct HomeEditorSheet: View {
    @Environment(\.dismiss) private var dismiss
    @Environment(AppModel.self) private var model
    @State private var order: [LightID] = []

    var body: some View {
        NavigationStack {
            List {
                Section {
                    if LightID.lights.count >= 2 { fixedRow("Tüm ışıklar", "lightswitch.on.fill", id: "all") }
                    fixedRow("Siri'ye ekle kartı", "mic.badge.plus", id: "siri")
                } header: { Text("Sabit kartlar") } footer: { Text("\"Tüm ışıklar\" en üstte, Siri kartı en altta durur.") }

                Section {
                    ForEach(order) { d in
                        HStack(spacing: 12) {
                            Image(systemName: d.symbol).frame(width: 26).foregroundStyle(Color.lampOn)
                            Text(d.title)
                            Spacer()
                            Toggle("", isOn: shownBinding(d.rawValue)).labelsHidden().tint(Color.lampOn)
                        }
                    }
                    .onMove { order.move(fromOffsets: $0, toOffset: $1); model.setHome(order: order) }
                } header: { Text("Cihazlar") } footer: {
                    Text("Sağdaki tutamaçlardan sürükleyerek sırala. Anahtarı kapatırsan kart ana ekrandan gizlenir; cihaz silinmez, widget, Siri ve otomasyonlarda kalır.")
                }

                Section {
                    Button("Varsayılan düzene dön", role: .destructive) { model.resetHome(); order = model.homeAllOrdered() }
                }
            }
            .environment(\.editMode, .constant(.active))
            .navigationTitle("Ana ekranı düzenle")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Bitti") { dismiss() } } }
            .onAppear { order = model.homeAllOrdered() }
        }
        .preferredColorScheme(.dark)
    }

    private func shownBinding(_ id: String) -> Binding<Bool> {
        Binding(get: { !model.homeHidden.contains(id) },
                set: { show in var h = model.homeHidden; if show { h.remove(id) } else { h.insert(id) }; model.setHome(hidden: h) })
    }

    private func fixedRow(_ title: String, _ symbol: String, id: String) -> some View {
        HStack(spacing: 12) {
            Image(systemName: symbol).frame(width: 26).foregroundStyle(Color.lampOn)
            Text(title)
            Spacer()
            Toggle("", isOn: shownBinding(id)).labelsHidden().tint(Color.lampOn)
        }
    }
}
