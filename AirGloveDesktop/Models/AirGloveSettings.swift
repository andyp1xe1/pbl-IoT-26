import Foundation

struct AirGloveSettings: Codable, Equatable {
    var sensitivityX: Double = 1.40
    var sensitivityY: Double = 1.20
    var deadzone: Double = 0.08
    
    var clickMapping: ClickMapping = .indexLeftMiddleRight
}

enum ClickMapping: String, Codable, CaseIterable, Identifiable {
    case indexLeftMiddleRight
    case indexLeftOnly
    case middleRightOnly
    
    var id: String {
        rawValue
    }
    
    var title: String {
        switch self {
        case .indexLeftMiddleRight:
            return "Index → Left, Middle → Right"
        case .indexLeftOnly:
            return "Index → Left"
        case .middleRightOnly:
            return "Middle → Right"
        }
    }
}
