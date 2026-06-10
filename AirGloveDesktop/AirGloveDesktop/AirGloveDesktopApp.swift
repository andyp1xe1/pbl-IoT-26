import SwiftUI

@main
struct AirGloveDesktopApp: App {
    @StateObject private var bluetooth = AirGloveBluetoothManager()
    @StateObject private var settingsStore = AirGloveSettingsStore()
    @StateObject private var calibrationStore = AirGloveCalibrationStore()
    @StateObject private var telemetryStore = AirGloveTelemetryStore()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bluetooth)
                .environmentObject(settingsStore)
                .environmentObject(calibrationStore)
                .environmentObject(telemetryStore)
                .frame(minWidth: 980, minHeight: 640)
        }
        .windowStyle(.titleBar)
    }
}
