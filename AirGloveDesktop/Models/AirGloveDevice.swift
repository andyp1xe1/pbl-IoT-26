//
//  AirGloveDevice.swift
//  AirGloveDesktop
//
//  Created by Crudu Alexandra on 09.06.2026.
//

import Foundation
import CoreBluetooth

struct AirGloveDevice: Identifiable, Equatable {
    let id: UUID
    let name: String
    let rssi: Int
    let peripheral: CBPeripheral
    
    var displayName: String {
        if name.isEmpty {
            return "Unknown BLE Device"
        }
        return name
    }
    
    var signalText: String {
        "\(rssi) dBm"
    }
    
    static func == (lhs: AirGloveDevice, rhs: AirGloveDevice) -> Bool {
        lhs.id == rhs.id
    }
}
