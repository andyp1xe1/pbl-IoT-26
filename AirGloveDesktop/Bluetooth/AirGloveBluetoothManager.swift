import Foundation
import CoreBluetooth
import Combine

final class AirGloveBluetoothManager: NSObject, ObservableObject {
    @Published var bluetoothStateText: String = "Bluetooth starting..."
    @Published var isScanning: Bool = false
    @Published var discoveredDevices: [AirGloveDevice] = []
    @Published var connectedDeviceName: String?
    @Published var statusText: String = "Not connected"
    @Published var selectedDeviceID: UUID?
    @Published var isConnecting: Bool = false
    @Published var hasConfigService: Bool = false
    
    private var centralManager: CBCentralManager?
    private var connectedPeripheral: CBPeripheral?
    private var telemetryCharacteristic: CBCharacteristic?
    private var settingsCharacteristic: CBCharacteristic?
    private var commandCharacteristic: CBCharacteristic?
    private var deviceInfoCharacteristic: CBCharacteristic?
    
    weak var telemetrySink: AirGloveTelemetryStore?
    
    private let maxVisibleDevices = 20
    
    override init() {
        super.init()
        
        centralManager = CBCentralManager(
            delegate: self,
            queue: .main
        )
    }
    
    func attachTelemetryStore(_ store: AirGloveTelemetryStore) {
        telemetrySink = store
    }
    
    func startScanning() {
        guard let centralManager else {
            statusText = "Bluetooth manager not ready"
            return
        }
        
        guard centralManager.state == .poweredOn else {
            statusText = "Bluetooth is not powered on"
            return
        }
        
        discoveredDevices.removeAll()
        isScanning = true
        statusText = "Scanning for Air Glove..."
        
        centralManager.scanForPeripherals(
            withServices: nil,
            options: [
                CBCentralManagerScanOptionAllowDuplicatesKey: false
            ]
        )
    }
    
    func stopScanning() {
        centralManager?.stopScan()
        isScanning = false
        
        if connectedDeviceName == nil && isConnecting == false {
            statusText = "Scan stopped"
        }
    }
    
    func connect(to device: AirGloveDevice) {
        guard let centralManager else {
            statusText = "Bluetooth manager not ready"
            return
        }
        
        guard centralManager.state == .poweredOn else {
            statusText = "Bluetooth is not powered on"
            return
        }
        
        stopScanning()
        
        selectedDeviceID = device.id
        isConnecting = true
        connectedDeviceName = nil
        connectedPeripheral = device.peripheral
        hasConfigService = false
        
        telemetryCharacteristic = nil
        settingsCharacteristic = nil
        commandCharacteristic = nil
        deviceInfoCharacteristic = nil
        
        device.peripheral.delegate = self
        
        statusText = "Connecting to \(device.displayName)..."
        
        centralManager.connect(
            device.peripheral,
            options: nil
        )
    }
    
    func disconnect() {
        guard let connectedPeripheral else {
            statusText = "No device connected"
            return
        }
        
        statusText = "Disconnecting..."
        centralManager?.cancelPeripheralConnection(connectedPeripheral)
    }
    
    func sendSettings(_ settings: AirGloveSettings) {
        guard let connectedPeripheral else {
            statusText = "Connect glove before sending settings"
            return
        }
        
        guard let settingsCharacteristic else {
            statusText = "Config service not found in firmware"
            return
        }
        
        let packet = AirGloveSettingsPacket(settings: settings)
        let data = packet.encoded()
        
        connectedPeripheral.writeValue(
            data,
            for: settingsCharacteristic,
            type: .withResponse
        )
        
        statusText = "Sending settings..."
    }
    
    func sendCommand(_ command: AirGloveCommand) {
        guard let connectedPeripheral else {
            statusText = "Connect glove before sending command"
            return
        }
        
        guard let commandCharacteristic else {
            statusText = "Config service not found in firmware"
            return
        }
        
        let data = Data([command.rawValue])
        
        connectedPeripheral.writeValue(
            data,
            for: commandCharacteristic,
            type: .withResponse
        )
        
        statusText = "Sending command..."
    }
    
    var isConnected: Bool {
        connectedPeripheral?.state == .connected
    }
    
    private func addOrUpdateDevice(
        peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi: NSNumber
    ) {
        let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let name = advertisedName ?? peripheral.name ?? ""
        
        guard isPossibleAirGloveName(name) else {
            return
        }
        
        let device = AirGloveDevice(
            id: peripheral.identifier,
            name: name,
            rssi: rssi.intValue,
            peripheral: peripheral
        )
        
        if let index = discoveredDevices.firstIndex(where: { $0.id == device.id }) {
            discoveredDevices[index] = device
        } else {
            discoveredDevices.append(device)
        }
        
        discoveredDevices.sort { $0.rssi > $1.rssi }
        
        if discoveredDevices.count > maxVisibleDevices {
            discoveredDevices = Array(discoveredDevices.prefix(maxVisibleDevices))
        }
    }
    
    private func isPossibleAirGloveName(_ name: String) -> Bool {
        let trimmed = name.trimmingCharacters(in: .whitespacesAndNewlines)
        
        guard trimmed.isEmpty == false else {
            return false
        }
        
        return trimmed.localizedCaseInsensitiveContains("AirGlove") ||
        trimmed.localizedCaseInsensitiveContains("Air Glove") ||
        trimmed.localizedCaseInsensitiveContains("AG-") ||
        trimmed.localizedCaseInsensitiveContains("Glove")
    }
}

extension AirGloveBluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .unknown:
            bluetoothStateText = "Bluetooth state unknown"
        case .resetting:
            bluetoothStateText = "Bluetooth resetting"
        case .unsupported:
            bluetoothStateText = "Bluetooth unsupported on this Mac"
        case .unauthorized:
            bluetoothStateText = "Bluetooth permission denied"
        case .poweredOff:
            bluetoothStateText = "Bluetooth is off"
            statusText = "Turn Bluetooth on"
        case .poweredOn:
            bluetoothStateText = "Bluetooth ready"
            statusText = "Ready to scan"
        @unknown default:
            bluetoothStateText = "Unknown Bluetooth state"
        }
    }
    
    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        addOrUpdateDevice(
            peripheral: peripheral,
            advertisementData: advertisementData,
            rssi: RSSI
        )
    }
    
    func centralManager(
        _ central: CBCentralManager,
        didConnect peripheral: CBPeripheral
    ) {
        isConnecting = false
        connectedPeripheral = peripheral
        connectedDeviceName = peripheral.name ?? "AirGlove"
        statusText = "Connected to \(connectedDeviceName ?? "AirGlove")"
        
        peripheral.delegate = self
        
        peripheral.discoverServices([
            AirGloveBLEProtocol.configServiceUUID
        ])
    }
    
    func centralManager(
        _ central: CBCentralManager,
        didFailToConnect peripheral: CBPeripheral,
        error: Error?
    ) {
        isConnecting = false
        connectedPeripheral = nil
        connectedDeviceName = nil
        selectedDeviceID = nil
        hasConfigService = false
        
        telemetrySink?.reset()
        
        if let error {
            statusText = "Failed to connect: \(error.localizedDescription)"
        } else {
            statusText = "Failed to connect"
        }
    }
    
    func centralManager(
        _ central: CBCentralManager,
        didDisconnectPeripheral peripheral: CBPeripheral,
        error: Error?
    ) {
        isConnecting = false
        connectedPeripheral = nil
        connectedDeviceName = nil
        selectedDeviceID = nil
        hasConfigService = false
        
        telemetryCharacteristic = nil
        settingsCharacteristic = nil
        commandCharacteristic = nil
        deviceInfoCharacteristic = nil
        
        telemetrySink?.reset()
        
        if let error {
            statusText = "Disconnected: \(error.localizedDescription)"
        } else {
            statusText = "Disconnected"
        }
    }
}

extension AirGloveBluetoothManager: CBPeripheralDelegate {
    func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverServices error: Error?
    ) {
        if let error {
            statusText = "Service discovery failed: \(error.localizedDescription)"
            return
        }
        
        guard let services = peripheral.services, services.isEmpty == false else {
            hasConfigService = false
            statusText = "Connected · Config service not found"
            return
        }
        
        for service in services {
            if service.uuid == AirGloveBLEProtocol.configServiceUUID {
                hasConfigService = true
                statusText = "Connected · Config service found"
                
                peripheral.discoverCharacteristics(
                    [
                        AirGloveBLEProtocol.telemetryCharacteristicUUID,
                        AirGloveBLEProtocol.settingsCharacteristicUUID,
                        AirGloveBLEProtocol.commandCharacteristicUUID,
                        AirGloveBLEProtocol.deviceInfoCharacteristicUUID
                    ],
                    for: service
                )
            }
        }
    }
    
    func peripheral(
        _ peripheral: CBPeripheral,
        didDiscoverCharacteristicsFor service: CBService,
        error: Error?
    ) {
        if let error {
            statusText = "Characteristic discovery failed: \(error.localizedDescription)"
            return
        }
        
        guard let characteristics = service.characteristics else {
            statusText = "No characteristics found"
            return
        }
        
        for characteristic in characteristics {
            switch characteristic.uuid {
            case AirGloveBLEProtocol.telemetryCharacteristicUUID:
                telemetryCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                
            case AirGloveBLEProtocol.settingsCharacteristicUUID:
                settingsCharacteristic = characteristic
                
            case AirGloveBLEProtocol.commandCharacteristicUUID:
                commandCharacteristic = characteristic
                
            case AirGloveBLEProtocol.deviceInfoCharacteristicUUID:
                deviceInfoCharacteristic = characteristic
                peripheral.readValue(for: characteristic)
                
            default:
                break
            }
        }
        
        if settingsCharacteristic != nil && commandCharacteristic != nil {
            statusText = "Connected · Ready for tuning"
        } else {
            statusText = "Connected · Config service incomplete"
        }
    }
    
    func peripheral(
        _ peripheral: CBPeripheral,
        didUpdateValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error {
            statusText = "Read/notify failed: \(error.localizedDescription)"
            return
        }
        
        guard let data = characteristic.value else {
            return
        }
        
        if characteristic.uuid == AirGloveBLEProtocol.telemetryCharacteristicUUID {
            if let packet = AirGloveTelemetryPacket.decode(from: data) {
                telemetrySink?.update(from: packet)
            }
        }
    }
    
    func peripheral(
        _ peripheral: CBPeripheral,
        didWriteValueFor characteristic: CBCharacteristic,
        error: Error?
    ) {
        if let error {
            statusText = "Write failed: \(error.localizedDescription)"
        } else {
            statusText = "Write successful"
        }
    }
}
