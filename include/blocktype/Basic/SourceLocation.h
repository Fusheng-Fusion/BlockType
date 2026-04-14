#pragma once

namespace blocktype {

class SourceLocation {
  unsigned ID = 0;
public:
  SourceLocation() = default;
  explicit SourceLocation(unsigned id) : ID(id) {}
  
  bool isValid() const { return ID != 0; }
  bool isInvalid() const { return ID == 0; }
  unsigned getID() const { return ID; }
  
  bool operator==(const SourceLocation &RHS) const { return ID == RHS.ID; }
  bool operator!=(const SourceLocation &RHS) const { return ID != RHS.ID; }
  bool operator<(const SourceLocation &RHS) const { return ID < RHS.ID; }
};

class SourceRange {
  SourceLocation Begin, End;
public:
  SourceRange() = default;
  SourceRange(SourceLocation B, SourceLocation E) : Begin(B), End(E) {}
  
  SourceLocation getBegin() const { return Begin; }
  SourceLocation getEnd() const { return End; }
  bool isValid() const { return Begin.isValid() && End.isValid(); }
};

} // namespace blocktype