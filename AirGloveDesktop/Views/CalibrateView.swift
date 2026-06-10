import SwiftUI

struct CalibrateView: View {
    @EnvironmentObject private var bluetooth: AirGloveBluetoothManager
    @EnvironmentObject private var calibrationStore: AirGloveCalibrationStore
    @EnvironmentObject private var telemetryStore: AirGloveTelemetryStore
    
    var body: some View {
        VStack(alignment: .leading, spacing: 24) {
            header
            
            sectionTitle("IMU — Live")
            
            VStack(spacing: 0) {
                HStack {
                    imuValue("Accel X", String(format: "%.2f", telemetryStore.telemetry.accelX))
                    imuValue("Accel Y", String(format: "%.2f", telemetryStore.telemetry.accelY))
                    imuValue("Accel Z", String(format: "%.2f", telemetryStore.telemetry.accelZ))
                }
                .padding()
                
                Divider()
                
                HStack {
                    imuValue("Gyro X", String(format: "%.3f", telemetryStore.telemetry.gyroX))
                    imuValue("Gyro Y", String(format: "%.3f", telemetryStore.telemetry.gyroY))
                    imuValue("Gyro Z", String(format: "%.3f", telemetryStore.telemetry.gyroZ))
                }
                .padding()
                
                Divider()
                
                HStack {
                    Text("Sample rate")
                    Spacer()
                    Text(sampleRateText)
                        .foregroundStyle(.secondary)
                }
                .padding()
            }
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            sectionTitle("Procedure")
            
            VStack(spacing: 0) {
                procedureRow(number: 1, text: "Place glove flat on a table", done: true)
                Divider()
                procedureRow(number: 2, text: "Hold still for 3 seconds", done: true)
                Divider()
                procedureRow(number: 3, text: "Tap Calibrate below", done: calibrationStore.lastIMUCalibrationDate != nil)
            }
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            VStack(spacing: 0) {
                Button {
                    calibrationStore.calibrateIMU()
                    bluetooth.sendCommand(.calibrateIMU)
                } label: {
                    Text("Calibrate IMU")
                        .font(.headline)
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
                .padding()
                
                Divider()
                
                Button {
                    calibrationStore.recalibrateTouchBaseline()
                    bluetooth.sendCommand(.calibrateTouchBaseline)
                } label: {
                    Text("Recalibrate touch baseline")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.plain)
                .padding()
            }
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            sectionTitle("Last calibration")
            
            VStack(spacing: 0) {
                infoRow("IMU", formattedDate(calibrationStore.lastIMUCalibrationDate))
                Divider()
                infoRow("Touch baseline", formattedDate(calibrationStore.lastTouchBaselineDate))
                Divider()
                infoRow("Status", calibrationStore.calibrationMessage)
            }
            .padding()
            .background(.white)
            .clipShape(RoundedRectangle(cornerRadius: 12))
            
            if bluetooth.connectedDeviceName == nil {
                Text("No glove connected. Calibration is saved locally for now.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            } else if bluetooth.hasConfigService == false {
                Text("Glove connected, but calibration commands require the future config service in firmware.")
                    .font(.footnote)
                    .foregroundStyle(.orange)
            }
            
            Spacer()
        }
        .padding(32)
        .background(Color(nsColor: .windowBackgroundColor))
        .navigationTitle("Calibrate")
    }
    
    private var header: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("Calibrate")
                .font(.system(size: 30, weight: .bold))
            
            Text(bluetooth.connectedDeviceName == nil ? "Not connected · local calibration only" : "Connected · \(bluetooth.hasConfigService ? "ready to calibrate" : "waiting for config service")")
                .font(.callout)
                .foregroundStyle(.secondary)
        }
    }
    
    private var sampleRateText: String {
        if telemetryStore.isDemoModeEnabled {
            return "\(telemetryStore.telemetry.sampleRate) Hz demo"
        }
        
        if telemetryStore.hasLiveTelemetry {
            return "\(telemetryStore.telemetry.sampleRate) Hz"
        }
        
        return "Offline"
    }
    
    private func sectionTitle(_ text: String) -> some View {
        Text(text.uppercased())
            .font(.caption)
            .foregroundStyle(.secondary)
    }
    
    private func imuValue(_ title: String, _ value: String) -> some View {
        VStack(spacing: 6) {
            Text(title.uppercased())
                .font(.caption2)
                .foregroundStyle(.secondary)
            
            Text(value)
                .font(.headline)
                .monospacedDigit()
        }
        .frame(maxWidth: .infinity)
    }
    
    private func procedureRow(number: Int, text: String, done: Bool) -> some View {
        HStack {
            Text("\(number)")
                .font(.headline)
                .foregroundStyle(.white)
                .frame(width: 30, height: 30)
                .background(done ? Color.green : Color.gray)
                .clipShape(RoundedRectangle(cornerRadius: 7))
            
            Text(text)
                .font(.headline)
            
            Spacer()
        }
        .padding()
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
