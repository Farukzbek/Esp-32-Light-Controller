import SwiftUI

/// Ilk acilista bir kez gosterilir: proje acik kaynak, kodlar ve kurulum belgeleri GitHub'da.
struct WelcomeView: View {
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(spacing: 22) {
            Spacer(minLength: 8)
            ZStack {
                Circle().fill(Color.lampOn).frame(width: 96, height: 96)
                    .shadow(color: Color.lampOn.opacity(0.5), radius: 24)
                Image(systemName: "power").font(.system(size: 42, weight: .semibold)).foregroundStyle(Color(hex: 0x14120C))
            }
            VStack(spacing: 8) {
                Text("Nowlight'a hoş geldin").font(.system(size: 26, weight: .bold, design: .rounded)).multilineTextAlignment(.center)
                Text("Odandaki ışıkları telefondan, widget'lardan ve Kısayollar'dan kontrol et. Evdeki WiFi'de çalışır, internet gerekmez.")
                    .font(.subheadline).foregroundStyle(.secondary).multilineTextAlignment(.center)
            }

            VStack(alignment: .leading, spacing: 12) {
                Label("Açık kaynak proje", systemImage: "chevron.left.forwardslash.chevron.right").font(.headline)
                Text("ESP32 yazılımı, köprü, bu uygulamanın kodu ve kurulum rehberi GitHub'da. Kendi ışık düğümlerini oradan yapıp ekleyebilirsin.")
                    .font(.subheadline).foregroundStyle(.secondary)
                Link(destination: AppInfo.repoURL) {
                    HStack {
                        Text("GitHub'da görüntüle").font(.subheadline.weight(.semibold))
                        Spacer()
                        Image(systemName: "arrow.up.right")
                    }
                    .padding(.horizontal, 14).padding(.vertical, 12)
                    .background(Color.white.opacity(0.1), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                }
                .foregroundStyle(.white)
            }
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.white.opacity(0.07), in: RoundedRectangle(cornerRadius: 20, style: .continuous))

            Text("Cihaz nasıl ekleneceğini Cihaz Ekle sekmesindeki rehberde bulabilirsin.")
                .font(.footnote).foregroundStyle(.secondary).multilineTextAlignment(.center)

            Spacer()
            Button { dismiss() } label: {
                Text("Başla").font(.headline).foregroundStyle(Color(hex: 0x14120C)).frame(maxWidth: .infinity).padding(.vertical, 14)
                    .background(Color.lampOn, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            }
        }
        .padding(24)
        .background(Color.black.ignoresSafeArea())
        .presentationDetents([.large])
        .interactiveDismissDisabled()          // "Başla"ya basmadan kapanmasın (kapaninca ayarlar acilir)
    }
}
