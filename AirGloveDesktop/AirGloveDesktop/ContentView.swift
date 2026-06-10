import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var bluetooth: AirGloveBluetoothManager
    @EnvironmentObject private var telemetryStore: AirGloveTelemetryStore
    
    var body: some View {
        NavigationSplitView {
            SidebarView()
        } detail: {
            DevicesView()
        }
        .onAppear {
            bluetooth.attachTelemetryStore(telemetryStore)
        }
    }
}

struct SidebarView: View {
    var body: some View {
        List {
            NavigationLink {
                DevicesView()
            } label: {
                Label("Devices", systemImage: "antenna.radiowaves.left.and.right")
            }
            
            NavigationLink {
                TuneView()
            } label: {
                Label("Tune", systemImage: "slider.horizontal.3")
            }
            
            NavigationLink {
                CalibrateView()
            } label: {
                Label("Calibrate", systemImage: "scope")
            }
            
            NavigationLink {
                AboutView()
            } label: {
                Label("About", systemImage: "info.circle")
            }
        }
        .navigationTitle("Air Glove")
    }
}

#Preview {
    ContentView()
        .environmentObject(AirGloveBluetoothManager())
        .environmentObject(AirGloveSettingsStore())
        .environmentObject(AirGloveCalibrationStore())
        .environmentObject(AirGloveTelemetryStore())
}
