import SwiftUI

struct DevicesView: View {
    @EnvironmentObject private var bluetooth: AirGloveBluetoothManager
    @EnvironmentObject private var telemetryStore: AirGloveTelemetryStore
    
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 26) {
                header
                
                connectionHero
                
                sectionTitle("Nearby devices")
                
                nearbyDevicesSection
                
                scanControls
                
                sectionTitle("Developer test")
                
                demoModeCard
                
                Text("Only devices named AirGlove, Air Glove, AG-, or Glove are shown. Anonymous BLE devices are hidden to keep the app stable.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            .padding(32)
        }
        .background(Color(nsColor: .windowBackgroundColor))
        .navigationTitle("Devices")
        .onDisappear {
            bluetooth.stopScanning()
        }
    }
    
    private var header: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Pair Air Glove")
                .font(.system(size: 36, weight: .bold))
            
            Text("Connect your Air Glove over Bluetooth Low Energy.")
                .font(.callout)
                .foregroundStyle(.secondary)
        }
    }
    
    private var connectionHero: some View {
        HStack(spacing: 18) {
            ZStack {
                RoundedRectangle(cornerRadius: 18)
                    .fill(heroColor)
                    .frame(width: 72, height: 72)
                
                Image(systemName: heroIcon)
                    .font(.system(size: 30, weight: .semibold))
                    .foregroundStyle(.white)
            }
            
            VStack(alignment: .leading, spacing: 6) {
                Text(heroTitle)
                    .font(.title3.bold())
                
                Text(heroSubtitle)
                    .foregroundStyle(.secondary)
            }
            
            Spacer()
            
            if bluetooth.isConnecting {
                ProgressView()
                    .controlSize(.regular)
            }
            
            if bluetooth.connectedDeviceName != nil {
                Button {
                    bluetooth.disconnect()
                } label: {
                    Text("Disconnect")
                        .frame(width: 110)
                }
                .buttonStyle(.bordered)
            }
        }
        .padding(22)
        .background(.white)
        .clipShape(RoundedRectangle(cornerRadius: 18))
    }
    
    private var heroColor: Color {
        if bluetooth.connectedDeviceName != nil {
            return .blue
        }
        
        if bluetooth.isConnecting {
            return .orange
        }
        
        return .gray
    }
    
    private var heroIcon: String {
        if bluetooth.connectedDeviceName != nil {
            return "checkmark"
        }
        
        if bluetooth.isConnecting {
            return "antenna.radiowaves.left.and.right"
        }
        
        return "hand.raised"
    }
    
    private var heroTitle: String {
        if let name = bluetooth.connectedDeviceName {
            return name
        }
        
        if bluetooth.isConnecting {
            return "Connecting..."
        }
        
        return "No Air Glove connected"
    }
    
    private var heroSubtitle: String {
        if bluetooth.connectedDeviceName != nil {
            if bluetooth.hasConfigService {
                return "Connected · tuning service available"
            } else {
                return "Connected · HID mode detected, config service not available yet"
            }
        }
        
        return "\(bluetooth.bluetoothStateText) · \(bluetooth.statusText)"
    }
    
    private var scanControls: some View {
        HStack(spacing: 12) {
            Button {
                if bluetooth.isScanning {
                    bluetooth.stopScanning()
                } else {
                    bluetooth.startScanning()
                }
            } label: {
                HStack {
                    Image(systemName: bluetooth.isScanning ? "stop.fill" : "arrow.clockwise")
                    Text(bluetooth.isScanning ? "Stop scanning" : "Scan for Air Glove")
                }
                .frame(maxWidth: .infinity)
            }
            .buttonStyle(.borderedProminent)
            
            Button {
                telemetryStore.startDemoMode()
            } label: {
                Text("Use demo")
                    .frame(width: 120)
            }
            .buttonStyle(.bordered)
            .disabled(telemetryStore.isDemoModeEnabled)
        }
    }
    
    private var nearbyDevicesSection: some View {
        VStack(spacing: 0) {
            if bluetooth.discoveredDevices.isEmpty {
                emptyNearbyState
            } else {
                ForEach(bluetooth.discoveredDevices) { device in
                    deviceRow(device)
                    
                    if device.id != bluetooth.discoveredDevices.last?.id {
                        Divider()
                            .padding(.leading, 68)
                    }
                }
            }
        }
        .background(.white)
        .clipShape(RoundedRectangle(cornerRadius: 16))
    }
    
    private var emptyNearbyState: some View {
        VStack(spacing: 14) {
            if bluetooth.isScanning {
                ProgressView()
                    .controlSize(.large)
            } else {
                Image(systemName: "dot.radiowaves.left.and.right")
                    .font(.system(size: 34))
                    .foregroundStyle(.secondary)
            }
            
            Text(bluetooth.isScanning ? "Searching for Air Glove..." : "No Air Glove found")
                .font(.headline)
            
            Text("Turn on the glove and keep it near your Mac. This is normal if the glove is not available right now.")
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .multilineTextAlignment(.center)
                .frame(maxWidth: 420)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 34)
        .padding(.horizontal, 20)
    }
    
    private func deviceRow(_ device: AirGloveDevice) -> some View {
        Button {
            bluetooth.connect(to: device)
        } label: {
            HStack(spacing: 14) {
                Image(systemName: "cpu")
                    .font(.title2)
                    .foregroundStyle(.white)
                    .frame(width: 42, height: 42)
                    .background(iconColor(for: device))
                    .clipShape(RoundedRectangle(cornerRadius: 10))
                
                VStack(alignment: .leading, spacing: 4) {
                    Text(device.displayName)
                        .font(.headline)
                        .foregroundStyle(.primary)
                    
                    Text(subtitle(for: device))
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                }
                
                Spacer()
                
                Text(device.signalText)
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
                
                if bluetooth.isConnecting && bluetooth.selectedDeviceID == device.id {
                    ProgressView()
                        .controlSize(.small)
                } else {
                    Image(systemName: bluetooth.selectedDeviceID == device.id ? "checkmark.circle.fill" : "chevron.right")
                        .foregroundStyle(bluetooth.selectedDeviceID == device.id ? .blue : .secondary)
                }
            }
            .padding()
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }
    
    private var demoModeCard: some View {
        HStack(spacing: 14) {
            Image(systemName: "waveform.path.ecg")
                .font(.title2)
                .foregroundStyle(.white)
                .frame(width: 42, height: 42)
                .background(telemetryStore.isDemoModeEnabled ? Color.green : Color.gray)
                .clipShape(RoundedRectangle(cornerRadius: 10))
            
            VStack(alignment: .leading, spacing: 4) {
                Text("Demo telemetry")
                    .font(.headline)
                
                Text(telemetryStore.isDemoModeEnabled ? "Fake live data is running" : "Use this when the glove is not available")
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
            
            Spacer()
            
            Button {
                if telemetryStore.isDemoModeEnabled {
                    telemetryStore.stopDemoMode()
                } else {
                    telemetryStore.startDemoMode()
                }
            } label: {
                Text(telemetryStore.isDemoModeEnabled ? "Stop" : "Start")
                    .frame(width: 80)
            }
            .buttonStyle(.bordered)
        }
        .padding()
        .background(.white)
        .clipShape(RoundedRectangle(cornerRadius: 16))
    }
    
    private func subtitle(for device: AirGloveDevice) -> String {
        if bluetooth.selectedDeviceID == device.id && bluetooth.isConnecting {
            return "Connecting..."
        }
        
        if bluetooth.selectedDeviceID == device.id && bluetooth.connectedDeviceName != nil {
            return "Connected"
        }
        
        return "Tap to connect"
    }
    
    private func iconColor(for device: AirGloveDevice) -> Color {
        if bluetooth.selectedDeviceID == device.id && bluetooth.connectedDeviceName != nil {
            return .blue
        }
        
        if bluetooth.selectedDeviceID == device.id && bluetooth.isConnecting {
            return .orange
        }
        
        return .gray
    }
    
    private func sectionTitle(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.caption)
            .foregroundStyle(.secondary)
    }
}
