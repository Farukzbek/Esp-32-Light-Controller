import AppIntents

/// Siri/Kisayollar'da secilen isik. Ad, kullanicinin verdigi adla dinamik gelir (AppEnum sabit olurdu).
struct LightEntity: AppEntity {
    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Işık" }
    static var defaultQuery = LightEntityQuery()

    var id: String
    var light: LightID { LightID(rawValue: id) ?? .masa }
    /// Siri adin tamamini ("Masa lambası") bekler; kisa soylenisler ("masa") esanlamli olarak eklenir.
    var displayRepresentation: DisplayRepresentation {
        DisplayRepresentation(title: "\(light.title)", image: .init(systemName: light.symbol),
                              synonyms: light.spokenNames.map { LocalizedStringResource(stringLiteral: $0) })
    }

    init(_ light: LightID) { id = light.rawValue }
}

extension LightID {
    /// Siri'de bu isigi anlatabilecek soylenisler (kucuk harf, tekrarsiz): tam ad, kisa ad, ilk kelime.
    var spokenNames: [String] {
        var out: [String] = []
        for n in [title, shortTitle, title.split(separator: " ").first.map(String.init) ?? ""] {
            let t = n.trimmingCharacters(in: .whitespaces)
            if !t.isEmpty, !out.contains(where: { $0.caseInsensitiveCompare(t) == .orderedSame }) { out.append(t) }
        }
        if self == .all { out += ["tümü", "hepsi", "tüm ışıklar"] }
        return out
    }
}

struct LightEntityQuery: EntityQuery, EntityStringQuery {
    private var choices: [LightID] { LightID.devices + (LightID.devices.count >= 2 ? [.all] : []) }
    func entities(for identifiers: [String]) async throws -> [LightEntity] {
        identifiers.compactMap { LightID(rawValue: $0) }.map(LightEntity.init)
    }
    func suggestedEntities() async throws -> [LightEntity] { choices.map(LightEntity.init) }

    /// Siri'nin duydugu kelimeyi tam ad, kisa ad ya da ilk kelimeyle eslestirir ("masa" -> "Masa lambası").
    func entities(matching string: String) async throws -> [LightEntity] {
        let q = string.lowercased(with: Locale(identifier: "tr_TR")).trimmingCharacters(in: .whitespaces)
        guard !q.isEmpty else { return choices.map(LightEntity.init) }
        return choices.filter { l in
            l.spokenNames.contains {
                let n = $0.lowercased(with: Locale(identifier: "tr_TR"))
                return n == q || n.contains(q) || q.contains(n)
            }
        }.map(LightEntity.init)
    }
}

/// Widget ayarinda bir yuva icin secenek: bir isik, "Tum isiklar" ya da "Bos". Adlar dinamik.
struct SlotEntity: AppEntity {
    static let emptyID = "bos"
    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Işık" }
    static var defaultQuery = SlotEntityQuery()

    var id: String
    var light: LightID? { LightID(rawValue: id) }
    var displayRepresentation: DisplayRepresentation {
        if let l = light { return DisplayRepresentation(title: "\(l.title)", image: .init(systemName: l.symbol)) }
        return DisplayRepresentation(title: "Boş", image: .init(systemName: "minus.circle"))
    }

    init(id: String) { self.id = id }
    init(_ light: LightID) { id = light.rawValue }
    static let empty = SlotEntity(id: emptyID)
}

struct SlotEntityQuery: EntityQuery {
    func entities(for identifiers: [String]) async throws -> [SlotEntity] {
        identifiers.filter { LightID(rawValue: $0) != nil || $0 == SlotEntity.emptyID }.map { SlotEntity(id: $0) }
    }
    func suggestedEntities() async throws -> [SlotEntity] {
        LightID.devices.map(SlotEntity.init) + (LightID.devices.count >= 2 ? [SlotEntity(.all)] : []) + [.empty]
    }
}
