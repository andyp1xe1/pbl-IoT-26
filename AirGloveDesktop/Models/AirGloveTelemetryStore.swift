import Foundation
import Combine

final class AirGloveTelemetryStore: ObservableObject {
    @Published var telemetry = AirGloveTelemetryPacket()
    @Published var hasLiveTelemetry: Bool = false
    @Published var isDemoModeEnabled: Bool = false
    
    private var demoTimer: Timer?
    
    func update(from packet: AirGloveTelemetryPacket) {
        telemetry = packet
        hasLiveTelemetry = true
    }
    
    func reset() {
        stopDemoMode()
        telemetry = AirGloveTelemetryPacket()
        hasLiveTelemetry = false
    }
    
    func startDemoMode() {
        stopDemoMode()
        
        isDemoModeEnabled = true
        hasLiveTelemetry = true
        
        demoTimer = Timer.scheduledTimer(withTimeInterval: 0.12, repeats: true) { [weak self] _ in
            self?.generateDemoPacket()
        }
    }
    
    func stopDemoMode() {
        demoTimer?.invalidate()
        demoTimer = nil
        isDemoModeEnabled = false
    }
    
    private func generateDemoPacket() {
        let time = Date().timeIntervalSince1970
        
        let thumb = UInt16(220 + Int.random(in: -30...30))
        let index = UInt16(620 + Int.random(in: -90...120))
        let middle = UInt16(420 + Int.random(in: -70...90))
        let ring = UInt16(180 + Int.random(in: -25...35))
        
        let accelX = Float(sin(time * 1.2) * 0.12)
        let accelY = Float(cos(time * 1.0) * 0.10)
        let accelZ = Float(9.79 + sin(time * 0.8) * 0.04)
        
        let gyroX = Float(sin(time * 2.0) * 0.018)
        let gyroY = Float(cos(time * 1.7) * 0.014)
        let gyroZ = Float(sin(time * 1.4) * 0.020)
        
        telemetry = AirGloveTelemetryPacket(
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
            sampleRate: 100
        )
    }
}
