//
//  AirGloveBLEProtocol.swift
//  AirGloveDesktop
//
//  Created by Crudu Alexandra on 09.06.2026.
//

import Foundation
import CoreBluetooth

enum AirGloveBLEProtocol {
    static let configServiceUUID = CBUUID(string: "A1B20000-7A3E-4C2D-9B1A-00A0C9000001")
    
    static let telemetryCharacteristicUUID = CBUUID(string: "A1B20001-7A3E-4C2D-9B1A-00A0C9000001")
    static let settingsCharacteristicUUID = CBUUID(string: "A1B20002-7A3E-4C2D-9B1A-00A0C9000001")
    static let commandCharacteristicUUID = CBUUID(string: "A1B20003-7A3E-4C2D-9B1A-00A0C9000001")
    static let deviceInfoCharacteristicUUID = CBUUID(string: "A1B20004-7A3E-4C2D-9B1A-00A0C9000001")
}

enum AirGloveCommand: UInt8 {
    case calibrateIMU = 1
    case calibrateTouchBaseline = 2
    case requestDeviceInfo = 3
    case requestCurrentSettings = 4
}

struct AirGloveSettingsPacket {
    let sensitivityX: Float
    let sensitivityY: Float
    let deadzone: Float
    let clickMapping: UInt8
    
    init(settings: AirGloveSettings) {
        self.sensitivityX = Float(settings.sensitivityX)
        self.sensitivityY = Float(settings.sensitivityY)
        self.deadzone = Float(settings.deadzone)
        self.clickMapping = settings.clickMapping.packetValue
    }
    
    func encoded() -> Data {
        var data = Data()
        data.appendFloat32LE(sensitivityX)
        data.appendFloat32LE(sensitivityY)
        data.appendFloat32LE(deadzone)
        data.append(clickMapping)
        return data
    }
}

struct AirGloveTelemetryPacket {
    var thumb: UInt16 = 0
    var index: UInt16 = 0
    var middle: UInt16 = 0
    var ring: UInt16 = 0
    
    var accelX: Float = 0
    var accelY: Float = 0
    var accelZ: Float = 0
    
    var gyroX: Float = 0
    var gyroY: Float = 0
    var gyroZ: Float = 0
    
    var sampleRate: UInt16 = 0
    
    static func decode(from data: Data) -> AirGloveTelemetryPacket? {
        var reader = AirGlovePacketReader(data: data)
        
        guard
            let thumb = reader.readUInt16LE(),
            let index = reader.readUInt16LE(),
            let middle = reader.readUInt16LE(),
            let ring = reader.readUInt16LE(),
            let accelX = reader.readFloat32LE(),
            let accelY = reader.readFloat32LE(),
            let accelZ = reader.readFloat32LE(),
            let gyroX = reader.readFloat32LE(),
            let gyroY = reader.readFloat32LE(),
            let gyroZ = reader.readFloat32LE(),
            let sampleRate = reader.readUInt16LE()
        else {
            return nil
        }
        
        return AirGloveTelemetryPacket(
            thumb: thumb,
            index: index,
            middle: middle,
            ring: ring,
            accelX: accelX,
            accelY: accelY,
            accelZ: accelZ,
            gyroX: gyroX,
            gyroY: gyroY,
            gyroZ: gyroZ,
            sampleRate: sampleRate
        )
    }
}

private struct AirGlovePacketReader {
    private let data: Data
    private var offset: Int = 0
    
    init(data: Data) {
        self.data = data
    }
    
    mutating func readUInt16LE() -> UInt16? {
        guard offset + 2 <= data.count else {
            return nil
        }
        
        let value = UInt16(data[offset]) | UInt16(data[offset + 1]) << 8
        offset += 2
        return value
    }
    
    mutating func readFloat32LE() -> Float? {
        guard offset + 4 <= data.count else {
            return nil
        }
        
        let value = UInt32(data[offset])
        | UInt32(data[offset + 1]) << 8
        | UInt32(data[offset + 2]) << 16
        | UInt32(data[offset + 3]) << 24
        
        offset += 4
        return Float(bitPattern: value)
    }
}

private extension Data {
    mutating func appendFloat32LE(_ value: Float) {
        let bits = value.bitPattern
        
        append(UInt8(bits & 0xFF))
        append(UInt8((bits >> 8) & 0xFF))
        append(UInt8((bits >> 16) & 0xFF))
        append(UInt8((bits >> 24) & 0xFF))
    }
}

extension ClickMapping {
    var packetValue: UInt8 {
        switch self {
        case .indexLeftMiddleRight:
            return 0
        case .indexLeftOnly:
            return 1
        case .middleRightOnly:
            return 2
        }
    }
}
