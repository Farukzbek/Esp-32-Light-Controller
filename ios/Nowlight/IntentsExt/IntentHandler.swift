import Intents

/// Siri komutlarini (kilitliyken de) arka planda calistirir: uygulamayi acmadan koprüye komut gonderir.
final class IntentHandler: INExtension, SetLightIntentHandling {
    override func handler(for intent: INIntent) -> Any { self }

    func handle(intent: SetLightIntent, completion: @escaping (SetLightIntentResponse) -> Void) {
        guard let raw = intent.light, let light = LightID(rawValue: raw), let on = intent.on?.boolValue else {
            completion(SetLightIntentResponse(code: .failure, userActivity: nil)); return
        }
        Task {
            do {
                try await LightActions.set(light, on: on)
                completion(SetLightIntentResponse(code: .success, userActivity: nil))
            } catch {
                completion(SetLightIntentResponse(code: .failure, userActivity: nil))
            }
        }
    }
}
