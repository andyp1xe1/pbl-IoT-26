import SwiftUI

struct TuneView: View {
    @EnvironmentObject private var bluetooth: AirGloveBluetoothManager
    @EnvironmentObject private var settingsStore: AirGloveSettingsStore
    @EnvironmentObject private var telemetryStore: AirGloveTelemetryStore
    
    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                header
                
                sectionTitle("Pointer")
                
                VStack(spacing: 0) {
                    sliderRow(
                        title: "Sensitivity X",
                        value: binding(\.sensitivityX),
                        range: 0.2...3.0,
                        suffix: "×"
                    )
                    
                    Divider()
                    
                    sliderRow(
                        title: "Sensitivity Y",
                        value: binding(\.sensitivityY),
                        range: 0.2...3.0,
                        suffix: "×"
                    )
                    
                    Divider()
                    
                    sliderRow(
                        title: "Dead zone",
                        value: binding(\.deadzone),
                        range: 0.0...0.30,
                        suffix: " rad/s"
                    )
                }
                .background(.white)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                
                sectionTitle("Touch — Live")
                
                VStack(spacing: 0) {
                    touchRow(
                        "Thumb",
                        value: Int(telemetryStore.telemetry.thumb),
                        progress: touchProgress(telemetryStore.telemetry.thumb),
                        isActive: telemetryStore.hasLiveTelemetry
                    )
                    
                    Divider()
                    
                    touchRow(
                        "Index",
                        value: Int(telemetryStore.telemetry.index),
                        progress: touchProgress(telemetryStore.telemetry.index),
                        isActive: telemetryStore.hasLiveTelemetry
                    )
                    
                    Divider()
                    
                    touchRow(
                        "Middle",
                        value: Int(telemetryStore.telemetry.middle),
                        progress: touchProgress(telemetryStore.telemetry.middle),
                        isActive: telemetryStore.hasLiveTelemetry
                    )
                    
                    Divider()
                    
                    touchRow(
                        "Ring",
                        value: Int(telemetryStore.telemetry.ring),
                        progress: touchProgress(telemetryStore.telemetry.ring),
                        isActive: telemetryStore.hasLiveTelemetry
                    )
                }
                .background(.white)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                
                Text(touchStatusText)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                
                sectionTitle("Mapping")
                
                VStack(spacing: 0) {
                    Picker("Click mapping", selection: binding(\.clickMapping)) {
                        ForEach(ClickMapping.allCases) { mapping in
                            Text(mapping.title).tag(mapping)
                        }
                    }
                    .pickerStyle(.menu)
                    .padding()
                    
                    Divider()
                    
                    HStack {
                        Text("Current mapping")
                        
                        Spacer()
                        
                        Text(settingsStore.settings.clickMapping.title)
                            .foregroundStyle(.secondary)
                    }
                    .padding()
                }
                .background(.white)
                .clipShape(RoundedRectangle(cornerRadius: 12))
                
                HStack {
                    Button {
                        settingsStore.resetToDefaults()
                    } label: {
                        Text("Reset to defaults")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.bordered)
                    
                    Button {
                        bluetooth.sendSettings(settingsStore.settings)
                    } label: {
                        Text("Send to glove")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.borderedProminent)
                    .disabled(bluetooth.connectedDeviceName == nil)
                }
                
                if bluetooth.connectedDeviceName == nil {
                    Text("Connect an Air Glove before sending settings to the device. For now, values are saved locally on this Mac.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                } else if bluetooth.hasConfigService == false {
                    Text("Glove connected, but config service is not available in the current firmware yet.")
                        .font(.footnote)
                        .foregroundStyle(.orange)
                }
            }
            .padding(32)
        }
        .background(Color(nsColor: .windowBackgroundColor))
        .navigationTitle("Tune")
    }
    
    private var header: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(bluetooth.connectedDeviceName ?? "Left Glove")
                .font(.system(size: 30, weight: .bold))
            
            Text(bluetooth.connectedDeviceName == nil ? "Not connected · local tuning only" : "Connected · \(bluetooth.hasConfigService ? "ready to tune" : "waiting for config service")")
                .font(.callout)
                .foregroundStyle(.secondary)
        }
    }
    
    private var touchStatusText: String {
        if telemetryStore.isDemoModeEnabled {
            return "Demo telemetry is running. Values are simulated."
        }
        
        if telemetryStore.hasLiveTelemetry {
            return "Live touch data is streaming from the glove."
        }
        
        return "Live values will appear after the firmware exposes the AirGlove telemetry characteristic."
    }
    
    private func binding<Value>(_ keyPath: WritableKeyPath<AirGloveSettings, Value>) -> Binding<Value> {
        Binding(
            get: {
                settingsStore.settings[keyPath: keyPath]
            },
            set: { newValue in
                settingsStore.settings[keyPath: keyPath] = newValue
            }
        )
    }
    
    private func sectionTitle(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.caption)
            .foregroundStyle(.secondary)
    }
    
    private func sliderRow(
        title: String,
        value: Binding<Double>,
        range: ClosedRange<Double>,
        suffix: String
    ) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack {
                Text(title)
                    .font(.headline)
                
                Spacer()
                
                Text(String(format: "%.2f%@", value.wrappedValue, suffix))
                    .foregroundStyle(.secondary)
                    .monospacedDigit()
            }
            
            Slider(value: value, in: range)
        }
        .padding()
    }
    
    private func touchProgress(_ value: UInt16) -> Double {
        min(Double(value) / 1023.0, 1.0)
    }
    
    private func touchRow(
        _ title: String,
        value: Int,
        progress: Double,
        isActive: Bool
    ) -> some View {
        HStack {
            Text(title)
                .frame(width: 90, alignment: .leading)
            
            ProgressView(value: progress)
                .frame(width: 180)
                .opacity(isActive ? 1 : 0.35)
            
            Spacer()
            
            Text("\(value)")
                .foregroundStyle(.secondary)
                .monospacedDigit()
        }
        .padding()
    }
}
