//
//  AirGloveSettingsStore.swift
//  AirGloveDesktop
//
//  Created by Crudu Alexandra on 09.06.2026.
//

import Foundation
import Combine

final class AirGloveSettingsStore: ObservableObject {
    @Published var settings: AirGloveSettings {
        didSet {
            save()
        }
    }
    
    private let storageKey = "airglove.settings.v1"
    
    init() {
        if let data = UserDefaults.standard.data(forKey: storageKey),
           let decoded = try? JSONDecoder().decode(AirGloveSettings.self, from: data) {
            self.settings = decoded
        } else {
            self.settings = AirGloveSettings()
        }
    }
    
    func resetToDefaults() {
        settings = AirGloveSettings()
    }
    
    private func save() {
        guard let data = try? JSONEncoder().encode(settings) else {
            return
        }
        
        UserDefaults.standard.set(data, forKey: storageKey)
    }
}
