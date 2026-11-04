#pragma once
inline bool loginAllowed(bool required, bool authenticated) { return !required || authenticated; }
inline bool writeAllowed(bool externalPower, bool tokenMatches) { return externalPower && tokenMatches; }
