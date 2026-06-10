import SwiftUI

struct AboutView: View {
    @EnvironmentObject private var bluetooth: AirGloveBluetoothManager
    @EnvironmentObject private var settingsStore: AirGloveSettingsStore
    @EnvironmentObject private var calibrationStore: AirGloveCalibrationStore
    @EnvironmentObject private var telemetryStore: AirGloveTelemetryStore
    
    var body: some View {
        VStack(alignment: .leading, spacing: 24) {
            Text("About")
                .font(.system(size: 30, weight: .bold))
            
            sectionTitle("Device")
            
            VStack(alignment: .leading, spacing: 14) {
                infoRow("Device name", bluetooth.connectedDeviceName ?? "Not connected")
                Divider()
                infoRow("Bluetooth", bluetooth.bluetoothStateText)
                Divider()
                infoRow("Connection", bluetooth.statusText)
                Divider()
                infoRow("Demo mode", telemetryStore.isDemoModeEnabled ? "On" : "Off")
                Divider()
                infoRow("BLE mode", "HID Mouse now · Config Service later")
            }
            .padding()
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            sectionTitle("Local settings")
            
            VStack(alignment: .leading, spacing: 14) {
                infoRow("Sensitivity X", String(format: "%.2f×", settingsStore.settings.sensitivityX))
                Divider()
                infoRow("Sensitivity Y", String(format: "%.2f×", settingsStore.settings.sensitivityY))
                Divider()
                infoRow("Dead zone", String(format: "%.2f rad/s", settingsStore.settings.deadzone))
                Divider()
                infoRow("Click mapping", settingsStore.settings.clickMapping.title)
            }
            .padding()
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            sectionTitle("Calibration")
            
            VStack(alignment: .leading, spacing: 14) {
                infoRow("Last IMU calibration", formattedDate(calibrationStore.lastIMUCalibrationDate))
                Divider()
                infoRow("Last touch baseline", formattedDate(calibrationStore.lastTouchBaselineDate))
            }
            .padding()
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            sectionTitle("Telemetry")
            
            VStack(alignment: .leading, spacing: 14) {
                infoRow("Live telemetry", telemetryStore.hasLiveTelemetry ? "Active" : "Inactive")
                Divider()
                infoRow("Sample rate", telemetryStore.hasLiveTelemetry ? "\(telemetryStore.telemetry.sampleRate) Hz" : "Offline")
                Divider()
                infoRow("Touch values", "Thumb \(telemetryStore.telemetry.thumb), Index \(telemetryStore.telemetry.index), Middle \(telemetryStore.telemetry.middle), Ring \(telemetryStore.telemetry.ring)")
            }
            .padding()
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            Text("This desktop app is prepared for pairing, tuning, calibration, and device inspection. Real tuning over BLE will require an AirGlove custom config service in the firmware.")
                .foregroundStyle(.secondary)
            
            Spacer()
        }
        .padding(32)
        .background(Color(nsColor: .windowBackgroundColor))
        .navigationTitle("About")
    }
    
    private func sectionTitle(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.caption)
            .foregroundStyle(.secondary)
    }
    
    private func infoRow(_ title: String, _ value: String) -> some View {
        HStack(alignment: .top) {
            Text(title)
            Spacer()
            Text(value)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.trailing)
        }
    }
    
    private func formattedDate(_ date: Date?) -> String {
        guard let date else {
            return "Never"
        }
        
        let formatter = DateFormatter()
        formatter.dateStyle = .medium
        formatter.timeStyle = .short
        
        return formatter.string(from: date)
    }
}
