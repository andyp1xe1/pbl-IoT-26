//
//  AirGloveCalibrationStore.swift
//  AirGloveDesktop
//
//  Created by Crudu Alexandra on 09.06.2026.
//

import Foundation
import Combine

final class AirGloveCalibrationStore: ObservableObject {
    @Published var lastIMUCalibrationDate: Date? {
        didSet {
            save()
        }
    }
    
    @Published var lastTouchBaselineDate: Date? {
        didSet {
            save()
        }
    }
    
    @Published var calibrationMessage: String = "No calibration performed yet."
    
    private let imuKey = "airglove.calibration.imu.date.v1"
    private let touchKey = "airglove.calibration.touch.date.v1"
    
    init() {
        let imuTime = UserDefaults.standard.double(forKey: imuKey)
        let touchTime = UserDefaults.standard.double(forKey: touchKey)
        
        if imuTime > 0 {
            lastIMUCalibrationDate = Date(timeIntervalSince1970: imuTime)
        } else {
            lastIMUCalibrationDate = nil
        }
        
        if touchTime > 0 {
            lastTouchBaselineDate = Date(timeIntervalSince1970: touchTime)
        } else {
            lastTouchBaselineDate = nil
        }
    }
    
    func calibrateIMU() {
        lastIMUCalibrationDate = Date()
        calibrationMessage = "IMU calibration saved locally."
    }
    
    func recalibrateTouchBaseline() {
        lastTouchBaselineDate = Date()
        calibrationMessage = "Touch baseline saved locally."
    }
    
    private func save() {
        if let lastIMUCalibrationDate {
            UserDefaults.standard.set(lastIMUCalibrationDate.timeIntervalSince1970, forKey: imuKey)
        }
        
        if let lastTouchBaselineDate {
            UserDefaults.standard.set(lastTouchBaselineDate.timeIntervalSince1970, forKey: touchKey)
        }
    }
}
