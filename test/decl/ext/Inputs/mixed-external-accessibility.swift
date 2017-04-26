internal extension Struct {
    public struct Inner: Hashable, Equatable {
        public var hashValue: Int { return 0 }
    }
}
internal func == (left: Struct.Inner, right: Struct.Inner) -> Bool { return true }
