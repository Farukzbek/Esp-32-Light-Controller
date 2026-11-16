import AppIntents

/// Siri/Kisayollar'da secilen isik. Ad, kullanicinin verdigi adla dinamik gelir (AppEnum sabit olurdu).
struct LightEntity: AppEntity {
    static var typeDisplayRepresentation: TypeDisplayRepresentation { "Işık" }
    static var defaultQuery = LightEntityQuery()

    var id: String
    var light: LightID { LightID(rawValue: id) ?? .masa }
    var displayRepresentation: DisplayRepresentation { DisplayRepresentation(title: "\(light.title)", image: .init(systemName: light.symbol)) }

    init(_ light: LightID) { id = light.rawValue }
}

struct LightEntityQuery: EntityQuery {
    private var choices: [LightID] { LightID.devices + (LightID.devices.count >= 2 ? [.all] : []) }
    func entities(for identifiers: [String]) async throws -> [LightEntity] {
        identifiers.compactMap { LightID(rawValue: $0) }.map(LightEntity.init)
    }
    func suggestedEntities() async throws -> [LightEntity] { choices.map(LightEntity.init) }
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
